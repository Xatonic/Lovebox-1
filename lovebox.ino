#include "settings.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Servo.h>
#include "SSD1306Wire.h"

const int fetchIntervalMillis = _fetchIntervalSeconds * 1000;
const char* ssid = _ssid;
const char* password = _password;
const String url = _url;
const int lightValueThreshold = _lightValueThreshold;

SSD1306Wire oled(0x3C, D2, D1);
Servo myservo; 
int pos = 90;
int increment = -1;
int lightValue;
String line;
String mode;
char idSaved = '0'; 
bool wasRead = true;  

void drawMessage(const String& message) {
  Serial.print("Drawing message....");
  oled.clear();

  // Unterscheide zwischen Text und Bild
  if(mode[0] == 't'){
    oled.drawStringMaxWidth(0, 0, 128, message);    
  } 
  else {
    for(int i = 0; i <= message.length(); i++){
      int x = i % 129;
      int y = i / 129;
    
      if(message[i] == '1'){
        oled.setPixel(x, y);
      }
    } 
  }    
  oled.display();
  Serial.println("done.");
}

void wifiConnect() {
  Serial.printf("Connecting to WiFi '%s'...", ssid);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }
  Serial.print("..done. IP ");
  Serial.println(WiFi.localIP());
}

void getGistMessage() {
  Serial.println("Fetching message...");
  const int httpsPort = 443;
  const char* host = "gist.githubusercontent.com";
  const char fingerprint[] = "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33";
  
  WiFiClientSecure client;
  client.setFingerprint(fingerprint);
  if (!client.connect(host, httpsPort)) {
    return; // Verbindung fehlgeschlagen
  }

  // add current millis as a cache-busting means
  client.print(String("GET ") + url + "?" + millis() + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String temp = client.readStringUntil('\n');
    if (temp == "\r") {
      break;
    }
  }
  String id = client.readStringUntil('\n');
  Serial.printf("\tid: '%s', last processed id: '%c'\n", id.c_str(), idSaved);
  if(id[0] != idSaved) { // new message
    wasRead = 0;
    idSaved = id[0];

    mode = client.readStringUntil('\n');
    Serial.println("\tmode: " + mode);
    line = client.readStringUntil(0);
    Serial.println("\tmessage: " + line);
    drawMessage(line);
  } else {
    Serial.println("\t-> message id wasn't updated");  
  }
}

void spinServo() {
  myservo.write(pos);      
  delay(50);    // wait 50ms to turn servo
  
  if(pos == 75 || pos == 105) { // 75°-105° range
    increment *= -1;
  }
  pos += increment;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n");

  Serial.print("Attaching servo...");
  myservo.attach(16);       // Servo an D0
  Serial.println("done.");

  Serial.print("Initializing display...");
  oled.init();
  oled.flipScreenVertically();
  oled.setColor(WHITE);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.setFont(ArialMT_Plain_10);
     
  oled.clear();
  oled.drawString(30, 30, "<3 LOVEBOX <3");
  oled.display();
  Serial.println("done.");
  
  wifiConnect();

  String readStateText = wasRead ? "was" : "wasn't";
  Serial.printf("Initial state: last processed id '%c' %s read.\n", idSaved, (wasRead ? "was" : "wasn't"));
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  if(wasRead) {
    getGistMessage();   
  }
  
  while(!wasRead) {   
    spinServo();
    lightValue = analogRead(0);
    if(lightValue > lightValueThreshold) {
      Serial.printf("Analog read value (LDR) %d above threshold of %d -> consider message read.\n", lightValue, lightValueThreshold);
      wasRead = 1;
    }
  }
  delay(fetchIntervalMillis);
}
