/*
   This source code is the property of Taw Tsv.
   Free for reuse.
   Made in fulfilment of a Third year project.
   Compiled in Arduino IDE 1.6.12.
*/


#include "ESP8266.h" //slightly modified this library
#include "SoftwareSerial.h"

#define SSID "Tawaz"
#define PASSWORD ""


//codes for http requests:
#define SEND_AT_COMMAND "/10" // send AT request
#define GET_HOME_INFO "/" //GET HOME PAGE
#define ON1 "/20" //- turn on 1
#define OFF1 "/21" //- turn off 1
#define ON2 "/22" //- turn on 2
#define OFF2 "/23" //- turn off 2
#define ON3 "/24" //- turn on 3
#define OFF3 "/25" //- turn off 3
#define ON4 "/26" //- turn on 4
#define OFF4 "/27" //- turn off 4
#define ON_ALL "/250" //- turn on all
#define OFF_ALL "/251" //- turn off all

//hardware pins usage
#define RX 2 //software serial pins
#define TX 3 //software serial pins for wifi module
#define CXN_LED 4 //is on when esp is connected to an AP
#define CURRENT_SENSOR A0 //ACS712 MODULE CONNECTED TO THIS PIN
#define switch1 8
#define switch2 9
#define switch3 10
#define switch4 11

//http request methods that are implemented by my server
#define GET "GET"
#define POST "POST"

#define BUFFER_SIZE 400 //512 seems to crash the godamn uno
#define WIFIBAUD 57600

//function declarations
int getCurrentFlowing();
uint16_t getFreeRam(void);
void printFreeRam(void);
boolean isWifiConnected(void);
boolean connectToAP(void);
boolean startServer(void);
int32_t indexOfChar(char source[], char target, uint32_t from);
int32_t indexOfStr(char source[], char target[], uint32_t from);
boolean isEqualStr(char s1[], char s2[]);
void retreiveHttpBody(char message[], char result[]);
void retreiveHttpUri(char message[], char result[]);
void sendATCommand(char command[], char result[]);
void getLine(char message[], char result[], uint32_t from);
void handleMessage(char message[], uint8_t mux_id);

SoftwareSerial mySerial(RX, TX);
ESP8266 wifi(mySerial);
uint8_t my_buffer[BUFFER_SIZE] = {0};

void setup() {
  pinMode(RX, INPUT);
  pinMode(TX, OUTPUT);
  pinMode(CXN_LED, OUTPUT);
  pinMode(switch1, OUTPUT);
  pinMode(switch2, OUTPUT);
  pinMode(switch3, OUTPUT);
  pinMode(switch4, OUTPUT);
  pinMode(CURRENT_SENSOR, INPUT_PULLUP);

  Serial.begin(WIFIBAUD);

  while (!Serial);
  
  delay(3000); //to allow wifi module to turn on before initialisation
  wifi.initialise(WIFIBAUD);
  connectToAP();
  digitalWrite(CXN_LED, LOW);
}
boolean serverStarted = false;
void loop() {
  if (!isWifiConnected()) {
    //reconnect if not connected and start server
    digitalWrite(CXN_LED, LOW); //show no connection on indicator
    wifi.initialise(WIFIBAUD);
    connectToAP();
    serverStarted = false;
    delay(2000);
    return;
  } else {
    digitalWrite(CXN_LED, HIGH);
  }
  if (!serverStarted) {
    serverStarted = startServer();
    return;
  }
  uint8_t mux_id;
  //response is higher as recv timeout increases.
  //5000 is a good value.
  //but smaller values < 500 ms lead to frequent
  //java.io.EOFException in android app.
  uint32_t len = wifi.recv(&mux_id, my_buffer, sizeof(my_buffer), 10000);
  if (len > 0) {
    handleMessage(my_buffer, mux_id);

    boolean released;
    uint8_t count = 0;
    while (count < 10) {
      released = wifi.releaseTCP(mux_id);
      if (released) break;
      else count++;
    }
    if (released) {
      Serial.println(F("release tcp ok"));
    } else {
      Serial.println(F("release tcp err"));
    }
  }
  delay(100); //take a break
}


int getCurrentFlowing() {
  //TODO current flowing in mA
  float average = 0, n = 256;
  int sensorValue;
  for (int i = 0; i < n; i++) {
    sensorValue = analogRead(CURRENT_SENSOR);
    if (sensorValue < 512) {
      //make negative value of current sensor positive
      sensorValue = 1024 - sensorValue;
    }
    if (sensorValue > average) average = sensorValue;
  }
  //average /= n;
  average = (average / 1024) * 5000;
  average -= 2544;
  average *= 10;
  return average;
}

uint16_t getFreeRam(void) {
  //function found on internet. will take it on faith.
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void printFreeRam(void) {
  Serial.print("RAm left: ");
  Serial.println(getFreeRam());
}

boolean isWifiConnected(void) {
  //returns true if we are connected to an AP
  if (!wifi.kick()) {
    delay(1000);
    return false;
  }
  String ip = wifi.getJoinedDeviceIP();
  ip.trim();
  if (ip.length() != 0) {
    return true;
  }
  sendATCommand("AT+CWJAP?", my_buffer);
  if (indexOfStr(my_buffer, "No AP", 0) == -1) return true;
  else return false;
}

boolean connectToAP(void) {
  Serial.println(F("Waiting for wifi module to turn on..."));
  while (!wifi.kick()) delay(1000); //wait until module is detected

  if (wifi.setOprToStationSoftAP()) {
    Serial.print(F("to station and AP ok\r\n"));
  } else {
    Serial.print(F("to station and AP err\r\n"));
  }
  if (wifi.setSoftAPParam("Tawaz Smart Home", "qwertyuiop")) {
    Serial.print(F("set SSID ok\r\n"));
  } else {
    Serial.print(F("set SSID err\r\n"));
  }

  
  boolean connectedSuccess = false;
  String ip = wifi.getJoinedDeviceIP();
  ip.trim();
  if (ip.length() != 0) {
    connectedSuccess = true;
  }
  if (!connectedSuccess) {
    //try to join AP
    wifi.joinAP(SSID, PASSWORD);
    connectedSuccess = true;
  }
  if (connectedSuccess) {
    Serial.print(F("Connected successfully\r\n"));
    Serial.print(F("IP: "));
    Serial.println(wifi.getLocalIP().c_str());
  } else {
    Serial.print(F("Not connected successfully\r\n"));
  }
  return connectedSuccess;
}

boolean startServer(void) {
  if (wifi.enableMUX()) {
    Serial.print(F("multiple ok\r\n"));
  } else {
    Serial.print(F("multiple err\r\n"));
  }
  boolean serverStarted = wifi.startServer(8090);
  if (serverStarted) {
    Serial.print(F("start tcp server ok\r\n"));
  } else {
    Serial.print(F("start tcp server err\r\n"));
  }

  if (wifi.setTCPServerTimeout(15)) {
    Serial.print(F("set tcp server timeout 10 seconds\r\n"));
  } else {
    Serial.print("set tcp server timeout err\r\n");
  }
  return serverStarted;
}

int32_t indexOfStr(char source[], char target[], uint32_t from) {
  //return start index of target substring in s array, search from 'from'.
  //return -1 if target substring is not in array
  char* pointer = strstr(source, target);
  if (pointer == NULL) return -1;
  return pointer - source;
}

int32_t indexOfChar(char source[], char target, uint32_t from) {
  //return index of target character in array s, search from 'from'.
  //return -1 if target is not in array
  for (uint32_t i = from; i < strlen(source); i++) {
    if (source[i] == target) return i;
  }
  return -1;
}

boolean isEqualStr(char s1[], char s2[]) {
  //returns true if s1 and s2 are the same length
  // and contain the same characters.
  //returns false otherwise
  return (strcmp(s1, s2) == 0);
}


void getLine(char message[], char result[], uint32_t from) {
  //get the line in message starting from up to newline or end of message
  //puts empty string in result if from is greater than message length - 1.
  if (strlen(message) - 1 < from) {
    result[0] = '\0';
    return;
  }
  //line ends with \r or \r\n
  uint32_t newlineIndex = indexOfChar(message, '\r', from);
  //line ends with \n
  if (newlineIndex == -1) {
    newlineIndex = indexOfChar(message, '\n', from);
  }
  if (newlineIndex == -1) newlineIndex = strlen(message);
  uint32_t count = 0;
  for (uint32_t i = from; i < newlineIndex; i++) {
    result[count] = message[i];
    count++;
  }
  result[count] = '\0';
}

void retreiveHttpBody(char message[], char result[]) {
  uint32_t startIndex, endIndex;
  //find double new line because body appears after that
  char crnl[] = "\r\n\r\n", nl[] = "\n\n", cr[] = "\r\r";
  startIndex = indexOfStr(message, crnl, 0);
  if (startIndex == -1) {
    startIndex = indexOfStr(message, nl, 0);
    if (startIndex == -1) {
      startIndex = indexOfStr(message, cr, 0);
      if (startIndex != -1) startIndex += strlen(cr);
    } else startIndex += strlen(nl);
  } else startIndex += strlen(crnl);
  uint32_t count = 0;
  for (uint32_t i = startIndex; i < strlen(message); i++) {
    result[count] = message[i];
    count++;
  }
  result[count] = '\0';

}

void retreiveHttpUri(char message[], char result[]) {
  //retreive Uri of http message.
  //returns empty string if uri not found.
  uint32_t startIndex, endIndex, getIndex;
  getIndex = indexOfStr(message, GET, 0);
  if (getIndex == -1) getIndex = indexOfStr(message, POST, 0);
  if (getIndex == -1) {
    result[0] = '\0'; //no uri
    return;
  }
  getLine(message, result, getIndex);
  startIndex = indexOfChar(result, ' ', getIndex); //next space
  if (startIndex == -1) {
    result[0] = '\0'; //no uri
    return;
  }
  startIndex += 1; //actual uri starts after space
  endIndex = indexOfChar(result, ' ', startIndex); // next space
  if (endIndex == -1) {
    result[0] = '\0'; //no uri
    return;
  }
  uint32_t count = 0;
  for (uint32_t i = startIndex; i < endIndex; i++) {
    result[count] = message[i];
    count++;
  }
  result[count] = '\0';
}

void sendATCommand(char command[], char result[]) {
  //sends at command and return string response.
  while (mySerial.available() > 0) mySerial.read(); //rxempty
  mySerial.println(command);
  char a;
  unsigned long start = millis();
  int readCount = 0;
  result[0] = '\0'; //start with empty string in result
  while (millis() - start < 10000) {
    while (mySerial.available() > 0 && readCount < BUFFER_SIZE - 1) {
      a = mySerial.read();
      if (a == '\0') continue;
      result[readCount] = (char)a;
      readCount++;
    }
    result[readCount] = '\0'; //terminate string at last read position
    if (indexOfStr(result, "OK", 0) != -1) {
      break;
    } else if (indexOfStr(result, "ERROR", 0) != -1) {
      break;
    } else if (indexOfStr(result, "FAIL", 0) != -1) {
      break;
    }
  }
}

void handleMessage(char message[], uint8_t mux_id) {
  //detect what is required by client with mux_id
  Serial.println(F("Received message: "));
  Serial.println(message);
  char uri[8] = ""; //handle only 8 characters in uri
  retreiveHttpUri(message, uri);
  boolean failed = false;
  if (strlen(uri) == 0) failed = true;

  String s = F("HTTP/1.0 200 OK\r\n"
               "Connection: Closed\r\n\r\n");
  String completestr = F("Action Complete\r\n");
  if (failed) {
    //instruction failed for some reason
    s += F("FAILED");
  }
  else if (isEqualStr(uri, ON1)) {
    digitalWrite(switch1, HIGH);
    s += completestr;
  }
  else if (isEqualStr(uri, OFF1)) {
    digitalWrite(switch1, LOW);
    s += completestr;
  }
  else if (isEqualStr(uri, ON2)) {
    digitalWrite(switch2, HIGH);
    s += completestr;
  }
  else if (isEqualStr(uri, OFF2)) {
    digitalWrite(switch2, LOW);
    s += completestr;
  }
  else if (isEqualStr(uri, ON3)) {
    digitalWrite(switch3, HIGH);
    s += F("Action Complete");
  }
  else if (isEqualStr(uri, OFF3)) {
    digitalWrite(switch3, LOW);
    s += completestr;
  }
  else if (isEqualStr(uri, ON4)) {
    digitalWrite(switch4, HIGH);
    s += completestr;
  }
  else if (isEqualStr(uri, OFF4)) {
    digitalWrite(switch4, LOW);
    s += completestr;
  }
  else if (isEqualStr(uri, ON_ALL)) {
    for (short i = switch1; i <= switch4; i++) {
      digitalWrite(i, HIGH);
    }
    s += completestr;
  }
  else if (isEqualStr(uri, OFF_ALL)) {
    for (short i = switch1; i <= switch4; i++) {
      digitalWrite(i, LOW);
    }
    s += completestr;
  }
  else if (isEqualStr(uri, SEND_AT_COMMAND)) {
    retreiveHttpBody(message, message);
    sendATCommand(message, message);
    for (uint32_t i = 0; i < strlen(message) && s.length() < BUFFER_SIZE; i++) {
      s += message[i];
    }
  }
  else if (isEqualStr(uri, GET_HOME_INFO)) {
    s += completestr;
    s += "free ram:";
    s += getFreeRam();
    s += "\r\n";
    s += "switches:";
    s += digitalRead(switch1);
    s += digitalRead(switch2);
    s += digitalRead(switch3);
    s += digitalRead(switch4);
    s += "\r\n";
    s += "current:";
    s += getCurrentFlowing();
    s += "\r\n";
  }
  //send back reply
  if (wifi.send(mux_id, s.c_str(), s.length()))
    Serial.println(F("Reply sent successfully"));
  else Serial.println(F("Reply sending was unsuccessful"));
}

