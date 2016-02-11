/////////////////////////////
// PICUS 802.11 ///// v1.2 //
// picus.steffenhartwig.de //
/////////////////////////////

#include <ESP8266WiFi.h>
#include "./DNSServer.h"                  // patched library
#include <ESP8266WebServer.h>

const byte        DNS_PORT = 53;          // capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // private network for server
DNSServer         dnsServer;              // create the DNS object
WiFiServer webServer(80);                 // HTTP server

WiFiUDP Udp;
unsigned int localPort = 7890;            // local port for listening in slave mode
IPAddress ipBroadcast (10, 10, 10, 255);  //broadcast adress for master mode

const int fetpin = 2;   // on this pin is the FET to drive the solenoid
const int zeit = 45;    // time in ms for the on-time of the solenoid
const int bl = 25;      // blink time
const int ledpin = 1;   // blue led on gpio1 (this is TX!) / inverted

const long heartrate = 30000; // check heartbeat every 30 seconds
long lastbeat = 0; // to store the time of the last check

boolean master = true;   //true-> master(ap)modus; false->join another network

int getRand(int startNum, int endNum) { // random number generator
  randomSeed(ESP.getCycleCount());
  return random(startNum, endNum);
}

void light(int anz) {  // "anz" short flashes of the blue led
  digitalWrite(ledpin, LOW);
  for (int i=1; i<anz; i++) {
    delay(bl);
    digitalWrite(ledpin, HIGH);
    delay(bl);
    digitalWrite(ledpin, LOW);
  }
  delay(bl);
  digitalWrite(ledpin, HIGH);
}

boolean findfriends() { // returns true if another picus network is present
  boolean nachbarn = false; //are there neighbors?
  WiFi.mode(WIFI_STA);      // station mode
  WiFi.begin("KNOCK KNOCK", "");  // join this network

  int versuche = 0;         // counter for tries
  while ((WiFi.status() != WL_CONNECTED) && (versuche < 25)) {   // try 25 times
    delay(350);
    light(1);
    versuche++;
  }
  nachbarn = (WiFi.status() == WL_CONNECTED);     // if connected -> there are neighbors
  return nachbarn;
}

void anfang() {
  if (master) {
    light(2);
    WiFi.mode(WIFI_AP);    // start AP
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("KNOCK KNOCK");
    dnsServer.start(DNS_PORT, "*", apIP); // DNS will reply local IP to all requests
    webServer.begin();
  } else {
    //if in slave mode:
    Udp.begin(localPort); //start listening
  }
}

void heartbleed() { // if not any longer connected -> try to reconnect
  Udp.stop();
  delay(getRand(0, 16000)); // a random delay of max. 16seconds to de-synchronize the clients
  master = !findfriends();   // decide if master or slave mode
  anfang();
}

void heartbeat() { // in slave mode-> check if still connected to network
  lastbeat = millis();
  if (WiFi.status() != WL_CONNECTED) {
    heartbleed();
  }
}

void knock() {  // the knock of the solenoid
  digitalWrite(fetpin, HIGH);
  delay(zeit);
  digitalWrite(fetpin, LOW);
}

void masterknock() {  // wraps the physical knock and the udp broadcast to the other desvices in master mmode
  Udp.beginPacket(ipBroadcast, 7890);
  Udp.write("#"); // content of the packet doesn't really matter at the moment
  Udp.endPacket();
  knock();  //conventional physical local knock
}

void setup() {
  pinMode(fetpin, OUTPUT); // the FET
  digitalWrite(fetpin, LOW);

  pinMode(ledpin, OUTPUT); // the LED
  digitalWrite(ledpin, HIGH);

  light(10);

  master = !findfriends();   // decide if master or slave mode
  anfang();
}

void loop() {
  if (master) {
    dnsServer.processNextRequest();
    WiFiClient client = webServer.available(); //check if somebody connected
    if (!client) {
      return;
    }
    while (!client.available()) { //wait for click/knock
      delay(1);
    }

    String request = client.readStringUntil('\r'); //read first line of the request
    client.flush();

    if (request.indexOf({"KLOPF"}) != -1) { // if request contains "KLOPF"
      masterknock();          // trigger master knock routine
    }

    // return the captive portal website
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(""); //  do not forget this one"
    client.println("<!DOCTYPE HTML>");
    client.println("<head>");
    client.println("  <style>");
    client.println("body {");
    client.println("      text-align: center;");
    client.println("      font-family:Helvetica, Arial, sans-serif;");
    client.println("      background: #eee;");
    client.println("      font-size: 12px;");
    client.println("    }");
    client.println("    #wrap {");
    client.println("      display: inline-block;");
    client.println("    }");
    client.println("    #wb {");
    client.println("      padding: 16px;");
    client.println("        border-style: solid;");
    client.println("        border-width: 8px;");
    client.println("        border-color: #f77;");
    client.println("    }");
    client.println("    #btn {");
    client.println("      font-size:64px;");
    client.println("      font-weight:bold;");
    client.println("      color: #f77;");
    client.println("      text-decoration: none;");
    client.println("      letter-spacing: 5px;");
    client.println("    }");
    client.println("    #brd {");
    client.println("      width: auto;");
    client.println("      text-align: center;");
    client.println("    }");
    client.println("    #txt {");
    client.println("      color: #777;");
    client.println("    }");
    client.println("    #ukid {");
    client.println("      font-weight: bold;");
    client.println("      font-size: 12px;");
    client.println("    }");
    client.println("    </style>");
    client.println("</head>");
    client.println("<html>");
    client.println("<br><br>");
    client.println("<div id=\"wrap\">");
    client.println("<div id=\"wb\">");
    client.println("<div id=\"brd\">");
    client.println("  <a id=\"btn\" href=\"/KLOPF\">KNOCK,<br>KNOCK.</a>");
    client.println("</div>");
    client.println("</div>");
    client.println("</div>");
    client.println("<div id=\"txt\">");
    client.println("  <p id=\"ukid\">picus 802.11</p>");
    client.println("  <p>a universal knocking intervention device</p>");
    client.println("  <p>don't worry, i'm harmless.</p>");
    client.println("</div>");
    client.println("</html>");

    delay(1);
  } else {
    // in slave mode ->
    int packetSize = Udp.parsePacket();  // check if UDP packet available
    if (packetSize) {     // if size > NULL
      knock();            // trigger physical knock
    }

    if ((millis() + heartrate) > lastbeat) { // if last heartbeat check more than 30s ago
      heartbeat();
    }
  }
}



