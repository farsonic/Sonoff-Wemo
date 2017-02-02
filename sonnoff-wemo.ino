//Modified 1 Feb 2017
//Adding in complete matching setup.xml file to ensure compatibitility
//Changed listenting port number to 49152 as this is what WeMo etc at looking to poll
//Alexa seems to want to retrive data on port 80 

// Modified 4 Jan 2016
// A-K Nathekar
// To include a momentary switch to switch relay on and off

// Modified 5 Jan 2016
// A-K Nathekar
// To include default power-on state to ON
// Changed ports for Sonoff
// Added LED Pin for Sonoff

// Modified 19 Jan 2016
// A-K Nathekar
// Fixed continual switch-on error.  Moved turnOnRelay(); in void loop() to end of void setup()

// Modified 21 Jan 2016
// A-K Nathekar
// Included Pin assignment constants for non-NodeMCU boards

// Modified 27 Jan 2016
// A-K Nathekar
// Corrected inversion of LED
// Moved default relay state from end of void setup() to immediately after prepareIds(); in setup()
// This is so that the relay is switched on before any WiFi connectivity is sought.
// Otherwise, the relay will only switch on after Wifi connectivity is established.
// Removed Delay(3000); in void setup();

// Set Arduino IDE to:
// Generic ESP8266 Module
// 80Mhz
// 40Mhz
// DIO
// 115200
// 1M (64K SPIFFS)
// ck
// disabled
// none

//Sonoff Pins
//
// Programmer  Sonoff (counting from top to bottom)
// 3V3       1 (Square PCB Pad)
// TX        2 (RX)
// RX        3 (TX)
// GND       4
// GPIO14    5

// Function       GPIO  NodeMCU
// Button           0      3
// Relay            12     6
// Green LED        13     7
// Spare (pin 5)    14     5


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>

void prepareIds();
boolean connectWifi();
boolean connectUDP();
void startHttpServer();
void turnOnRelay();
void turnOffRelay();

const char* ssid = "ssid";
const char* password = "password";

unsigned int localPort = 1900;      // local port to listen on

WiFiUDP UDP;
boolean udpConnected = false;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;      // local port to listen on

ESP8266WebServer HTTP(49152);
 
boolean wifiConnected = false;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

String serial;
String persistent_uuid;

// ***  Change device name below to suit your naming convention *** 
String device_name = "Office Light";

// For NodeMCU boards, use these constants
// const int relayPin  = D6;
// const int switchPin = D3;
// const int ledPin    = D7;

// For non-NodeMCU boards (Sonoff, Wemos), use these constants
const int relayPin  = 12;
const int switchPin = 0;
const int ledPin    = 13;

int switchState = 0;

boolean cannotConnectToWifi = false;

void setup() {
  Serial.begin(115200);

  // Setup Relay
  pinMode(relayPin, OUTPUT);

  // Setup Switch
  pinMode(switchPin, INPUT);  

  // Setup LED
  pinMode(ledPin, OUTPUT);

//Default Relay state is on when unit is initially powered on
  turnOffRelay();
  switchState = 0;
  Serial.println("Setting relay to off - setting value to 0");

  prepareIds();

//  delay(3000);
  // Initialise wifi connection
  wifiConnected = connectWifi();

  // only proceed if wifi connection successful
  if(wifiConnected){
    udpConnected = connectUDP();
    
    if (udpConnected){
      // initialise pins if needed 
      startHttpServer();
    }
  }  
}

void loop() {
 
// Poll Switch pin.  If True (i.e. pressed), toggle switchState variable and set relay to that state
// Only change relay state if switch goes High and subsequently Low.
// So, the effect is that upon releasing the button, the relay will change state.
// This is the same way the original Sonoff firmware works and 
// the quickest way I could devise a debounce routine.
// Also, send the changed state to the serial port.

if (digitalRead(switchPin)){
  delay(250);
  if (!digitalRead(switchPin)){
  switchState = !switchState;
  Serial.print("Switch Pressed - Relay now "); 

// Show and change Relay State  
  if (switchState){
    Serial.println("ON");
     turnOnRelay();
  }
    else
    {
    Serial.println("OFF");
     turnOffRelay();
    }
  delay(500);
  }
  }

  HTTP.handleClient();
  delay(1);
  
  
  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
    if(udpConnected){    
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      
      if(packetSize) {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        
        for (int i =0; i < 4; i++) {
          Serial.print(remote[i], DEC);
          if (i < 3) {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());
        
        int len = UDP.read(packetBuffer, 255);
        
        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;
        //Serial.println("Request:");
        //Serial.println(request);
         
        if(request.indexOf('M-SEARCH') > 0) {
            if(request.indexOf("urn:Belkin:device:*") > 0) {
                Serial.println("Responding to search request ...");
                respondToSearch();
            }
        }
      }
        
      delay(10);
    }
  } else {
      // Turn on/off to indicate cannot connect ..      
  }
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistent_uuid = "Socket-1_0-" + serial;
//  device_name = "TestDevice";
}

void respondToSearch() {
    Serial.println("");
    Serial.print("Sending response to ");
    Serial.println(UDP.remoteIP());
    Serial.print("Port : ");
    Serial.println(UDP.remotePort());

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response = 
         "HTTP/1.1 200 OK\r\n" 
         "CONTENT-LENGTH: 4413\r\n"
         "CONTENT-TYPE: text/xml\r\n"
         "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":49152/setup.xml\r\n"
         "LAST-MODIFIED: Thu, 19 Jan 2017 14:29:54 GMT\r\n"
         "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
         "X-User-Agent: redsonic\r\n\r\n";

    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.write(response.c_str());
    UDP.endPacket();                    

     Serial.println("Response sent !");
}

void startHttpServer() {
    HTTP.on("/index.html", HTTP_GET, [](){
      Serial.println("Got Request index.html ...\n");
      HTTP.send(200, "text/plain", "Hello World!");
    });

    HTTP.on("/upnp/control/basicevent1", HTTP_POST, []() {
      Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");      ;


      //for (int x=0; x <= HTTP.args(); x++) {
      //  Serial.println(HTTP.arg(x));
      //}
  
      String request = HTTP.arg(0);      
      Serial.print("request:");
      Serial.println(request);
 
      if(request.indexOf("<BinaryState>1</BinaryState>") > 0) {
          Serial.println("Got Turn on request");       
          
           Serial.print (switchState); 
           turnOnRelay();
          
      }

      if(request.indexOf("<BinaryState>0</BinaryState>") > 0) {
          Serial.println("Got Turn off request");
          
           Serial.print (switchState);
           turnOffRelay();
          
      }
      
      HTTP.send(200, "text/plain", "");
    });

    HTTP.on("/eventservice.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to eventservice.xml ... ########\n");
      String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
            "<actionList>"
              "<action>"
                "<name>SetBinaryState</name>"
                "<argumentList>"
                  "<argument>"
                    "<retval/>"
                    "<name>BinaryState</name>"
                    "<relatedStateVariable>BinaryState</relatedStateVariable>"
                    "<direction>in</direction>"
                  "</argument>"
                "</argumentList>"
                 "<serviceStateTable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>BinaryState</name>"
                    "<dataType>Boolean</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>level</name>"
                    "<dataType>string</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                "</serviceStateTable>"
              "</action>"
            "</scpd>\r\n"
            "\r\n";
            
      HTTP.send(200, "text/plain", eventservice_xml.c_str());
    });
    
    HTTP.on("/setup.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to setup.xml ... ########\n");

      IPAddress localIP = WiFi.localIP();
      char s[16];
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    
      String setup_xml = "<?xml version=\"1.0\"?>"
            "<root xmlns=\"urn:Belkin:device-1-0\">"
            "<specVersion>"
              "<major>1</major>"
              "<minor>0</minor>"
            "</specVersion>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ device_name +"</friendlyName>"
                    "<manufacturer>Belkin International Inc.</manufacturer>"
                    "<manufacturerURL>http://www.belkin.com</manufacturerURL>"
                    "<modelDescription>Belkin Plugin Socket 1.0</modelDescription>"
                    "<modelName>Socket</modelName>"
                    "<modelNumber>1.0</modelNumber>"
                    "<modelURL>http://www.belkin.com/plugin/</modelURL>"
                "<serialNumber>221517K0101769</serialNumber>"                
                "<UDN>uuid:"+ persistent_uuid +"</UDN>"
                    "<UPC>123456789</UPC>"
                "<macAddress>5ccf7f1cafad</macAddress>"
                "<firmwareVersion>WeMo_WW_2.00.10626.PVT-OWRT-SNS</firmwareVersion>"
                "<iconVersion>0|49153</iconVersion>"
                "<iconList>"
                    "<icon>"
                           "<mimetype>jpg</mimetype>"
                           "<width>100</width>"
                           "<height>100</height>"
                           "<depth>100</depth>" 
                           "<url>icon.jpg</url>"
                     "</icon>"
                "</iconList>"
                "<serviceList>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:WiFiSetup:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:WiFiSetup1</serviceId>"
                    "<controlURL>/upnp/control/WiFiSetup1</controlURL>"
                    "<eventSubURL>/upnp/event/WiFiSetup1</eventSubURL>"
                    "<SCPDURL>/setupservice.xml</SCPDURL>"
                    "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:timesync:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:timesync1</serviceId>"
                    "<controlURL>/upnp/control/timesync1</controlURL>"
                    "<eventSubURL>/upnp/event/timesync1</eventSubURL>"
                    "<SCPDURL>/timesyncservice.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                    "<controlURL>/upnp/control/basicevent1</controlURL>"
                    "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                    "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:firmwareupdate:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:firmwareupdate1</serviceId>"
                    "<controlURL>/upnp/control/firmwareupdate1</controlURL>"
                    "<eventSubURL>/upnp/event/firmwareupdate1</eventSubURL>"
                    "<SCPDURL>/firmwareupdate.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:rules:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:rules1</serviceId>"
                    "<controlURL>/upnp/control/rules1</controlURL>"
                    "<eventSubURL>/upnp/event/rules1</eventSubURL>"
                    "<SCPDURL>/rulesservice.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:metainfo:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:metainfo1</serviceId>"
                    "<controlURL>/upnp/control/metainfo1</controlURL>"
                    "<eventSubURL>/upnp/event/metainfo1</eventSubURL>"
                    "<SCPDURL>/metainfoservice.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:remoteaccess:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:remoteaccess1</serviceId>"
                    "<controlURL>/upnp/control/remoteaccess1</controlURL>"
                    "<eventSubURL>/upnp/event/remoteaccess1</eventSubURL>"
                    "<SCPDURL>/remoteaccess.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:deviceinfo:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:deviceinfo1</serviceId>"
                    "<controlURL>/upnp/control/deviceinfo1</controlURL>"
                    "<eventSubURL>/upnp/event/deviceinfo1</eventSubURL>"
                    "<SCPDURL>/deviceinfoservice.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:smartsetup:1</serviceType>"
                    "<serviceId>urn:Belkin:serviceId:smartsetup1</serviceId>"
                    "<controlURL>/upnp/control/smartsetup1</controlURL>"
                    "<eventSubURL>/upnp/event/smartsetup1</eventSubURL>"
                    "<SCPDURL>/smartsetup.xml</SCPDURL>"
                  "</service>"
                  "<service>"
                    "<serviceType>urn:Belkin:service:manufacture:1</serviceType>"
                     "<serviceId>urn:Belkin:serviceId:manufacture1</serviceId>"
                     "<controlURL>/upnp/control/manufacture1</controlURL>"
                     "<eventSubURL>/upnp/event/manufacture1</eventSubURL>"
                     "<SCPDURL>/manufacture.xml</SCPDURL>"
                   "</service>"
              "</serviceList>"
              "<presentationURL>/pluginpres.html</presentationURL>"
              "</device>"
            "</root>\r\n"
            "\r\n";
            
        HTTP.send(200, "text/xml", setup_xml.c_str());
        
        Serial.print("Sending :");
        Serial.println(setup_xml);
    });
    
    HTTP.begin();  
    Serial.println("HTTP Server started ..");
}


      
// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

boolean connectUDP(){
  boolean state = false;
  
  Serial.println("");
  Serial.println("Connecting to UDP");
  
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }
  
  return state;
}

void turnOnRelay() {
  digitalWrite(relayPin, HIGH); // turn on relay with voltage HIGH 
  digitalWrite(ledPin, LOW  ); // turn on LED with voltage HIGH 
  Serial.println("Turning on relay");
  switchState = 1;
}

void turnOffRelay() {
  digitalWrite(relayPin, LOW);  // turn off relay with voltage LOW
  digitalWrite(ledPin, HIGH);  // turn off LED with voltage LOW
  Serial.println("Turning off relay");
  switchState = 0;

}
