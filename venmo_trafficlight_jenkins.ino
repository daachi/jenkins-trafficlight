/*
  Venmo Jenkins client
 
 This Arduino program connects to Venmo's Jenkins instance
 and activates certain output pins (meant to be plugged into
 relays which drive a traffic light) to correspond to certain
 test states:
    - Red: Last test run did not pass
    - Yellow: Tests currently running
    - Green: Last test run passed
    (Note that exactly one of red or green will be on, but
    yellow is independent of these.)
 
 REQUIRES: Arduino Wiznet Ethernet shield
 
 Startup Sequence:
  The traffic light will light up in this sequence to indicate
  boot status:
  
  Red - Arduino has powered up, this program is running
  Red+Yellow - Arduino has acquired DHCP address from the network
  Red+Yellow+Green - Network completely configured, program start
  All lights will turn off, and Main Loop commences.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Traffic light relay circuits connected to:
   * Red - pin 7
   * Yellow/Amber - pin 6
   * Green - pin 5
 
 created 05 Jun 2012 - 25 Jul 2013
 by Peyton Sherwood

 */

#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {  0x90, 0xA2, 0xDA, 0x0D, 0x33, 0x0A }; // my MAC address from my sticker
IPAddress server(184,73,153,84); // Jenkins server

int redled = 7; // pin 7
int ambled = 6; // pin 6
int grnled = 5; // pin 5

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

void setup() {
  pinMode(redled, OUTPUT);
  pinMode(ambled, OUTPUT);
  pinMode(grnled, OUTPUT);
  digitalWrite(redled, LOW);
  digitalWrite(ambled, LOW);
  digitalWrite(grnled, LOW);
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
   }
   digitalWrite(redled, HIGH);  // Get Ready

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;) {
      flipLed(redled);
      delay(1000);
    }
  }
  
  digitalWrite(ambled, HIGH);
  Serial.println("My IP address: ");
  Serial.println(Ethernet.localIP());

  // give the Ethernet shield a second to initialize:
  delay(1000);

  digitalWrite(grnled, HIGH);
  delay(500);
  digitalWrite(redled, LOW);
  digitalWrite(ambled, LOW);
  digitalWrite(grnled, LOW);
}

void loop()
{
  int isBuilding = getBuildingStatus();
  Serial.println("building status: ");
  Serial.println(isBuilding);

  int buildSucceeded = getBuildSuccess();
  Serial.println("build succeeded: ");
  Serial.println(buildSucceeded);
  
  if (isBuilding == -1 || buildSucceeded == -1) {
    Serial.println("blinking");
    blink(redled);
    Serial.println("notblinking");
    return;
  }
  if (isBuilding == 1) {
    digitalWrite(ambled, HIGH);
  } else {
    digitalWrite(ambled, LOW);
  }
  if (buildSucceeded == 1) {
    digitalWrite(redled, LOW);
    digitalWrite(grnled, HIGH);
  } else {
    digitalWrite(redled, HIGH);
    digitalWrite(grnled, LOW);
  }
  
  delay(10000);
}

void flipLed(int led) {
  digitalWrite(led, !digitalRead(led));
}

// the loop routine runs over and over again forever:
void blink(int ledid) {
  for (int i = 0; i < 5; i++) {
    digitalWrite(ledid, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);               // wait for a second
    digitalWrite(ledid, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);               // wait for a second
  }
}

int getBuildingStatus() {
  String urlpart1 = "/job/venmo_platform_master/lastBuild/api/xml?xpath=/";
  String url = urlpart1 + "*/building";
  String xmltag = "<building>";
  String truevalue = "true";
  return getValueFromJenkins(url, xmltag, truevalue);
}

int getBuildSuccess() {
  String url1 = "/job/venmo_platform_master/lastCompletedBuild/api/xml?xpath=/";
  String url = url1 + "*/result";
  String xmltag = "<result>";
  String truevalue = "SUCCESS";
  return getValueFromJenkins(url, xmltag, truevalue);
}

int getValueFromJenkins(String url, String xmltag, String truevalue) {
  Serial.println("connecting...");
  int retval = -1;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    String method = "GET ";
    String request = method + url;
    String fullreq = request + " HTTP/1.0";
    client.println(fullreq);
    Serial.println(fullreq);
    client.println("Authorization: Basic dHJhZmZpY2xpZ2h0OnRyYWZmaWNsaWdodA==");
    Serial.println("Authorization: Basic dHJhZmZpY2xpZ2h0OnRyYWZmaWNsaWdodA==");
    client.println("Host: jenkins.venmo.com");
    Serial.println("Host: jenkins.venmo.com");
    client.println();
  } 
  else {
    // kf you didn't get a connection to the server:
    Serial.println("connection failed");
    for (int i=0; i<3; i++) {
      flipLed(redled);
      delay(200);
    }
    return -1;
  }
  
  String buf = "";
  while(client.connected()) {
    while (client.available()) {
      char c = client.read();
      buf = buf + c;
      if (c == 0x0A) { // newline
        int isbuildingresult = checkBufXmlValue(buf, xmltag, truevalue);
        Serial.print(buf);
        // Serial.println(isbuildingresult);
        buf = "";
        if (isbuildingresult > -1) {
          retval = isbuildingresult;
        }
      }
    }
    delay(50);
  }
  int isbuildingresult = checkBufXmlValue(buf, xmltag, truevalue);
  Serial.print(buf);
  Serial.println(isbuildingresult);
  buf = "";
  if (isbuildingresult > -1) {
    retval = isbuildingresult;
  }
    
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
  return retval; 
}

int checkBufXmlValue(String buf, String xml, String truevalue) {
  // checks buf to see if it contains a particular xml tag xml
  // then returns whether the truevalue is the value within the tag
  if (buf.length() > xml.length() &&
      buf.substring(0,xml.length()) == xml) {
    if (buf.length() >= xml.length() + truevalue.length() &&
        buf.substring(xml.length(), xml.length()+truevalue.length()) == truevalue) {
      return 1;
    }
    return 0;
  }
  return -1;
}

