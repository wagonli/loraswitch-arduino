#include <SeeedOLED.h>
#include <Wire.h>
#include <math.h>
#define MODE_OTAA

#define SerialDebug Serial
#define SerialLoRa Serial1

#define ADC_AREF 3.3
#define BATVOLTPIN A6
#define BATVOLT_R1 4.7
#define BATVOLT_R2 10

const unsigned long ULDelayLoop = 180000; // ERC7003 868 MHz 1% duty cycle   SF12 5bytes
const unsigned long ULAirTimeDelay = 2100; // class A max air time is 2.1sec
const unsigned long LoRaSerialDelay = 1000;
const unsigned long LCDDelay = 250;

const int LED_PCB_PIN = 9;
const int PIN_POWERLINE = A4;
const int RELAY_EXT_PIN = 4;
const int LCD_LINE_NR = 16;
const int LCD_DISPLAY_MOD = 10;

unsigned long elapsedTimeSinceLastSending = -ULDelayLoop;

void changeRelayState(bool state);
void initLCD();
void displayOnLCDXY(int X, int Y, String string);
void displayOnLCDX(int X, String string);
void initLoRaModule();
void sendATCommandToLoRa(String upData, bool extraDelay, String &downData);
void blinkLed(int seconds);
void sendPowerLineValuetoLoRa(byte value, String &result);
void sendDatatoLoRa(String data, String &result);
void updateLedStatus(int led, boolean predicate);
byte digitizePowerValue(int powerValue);
int readPowerInput();
float getRealBatteryVoltage();
void displayStringInHexChar(String data);

void setup() {
  // *********** init Debug and LoRa baud rate ***********************************
  SerialDebug.begin(19200);      // the debug baud rate on Hardware Serial Atmega
  SerialLoRa.begin(19200);       // the LoRa baud rate on Software Serial Atmega

  Wire.begin();
  initLCD();
  displayOnLCDX(4, "Please Wait...");

  SerialDebug.println("**************** LoraSwitch ***********************");

  // *********** init digital pins **********************************************
  pinMode(LED_PCB_PIN, OUTPUT);
  digitalWrite(LED_PCB_PIN, LOW);  //init
  pinMode(RELAY_EXT_PIN, OUTPUT);
  digitalWrite(RELAY_EXT_PIN, HIGH);  //init

  SerialDebug.println("LoRa Switch Init...");
  blinkLed(3);

  // set ATO & ATM Parameters for Nano N8
  initLoRaModule();

  delay(2000);
  SerialDebug.println("Init Completed...");
  displayOnLCDX(4, " ");
  digitalWrite(RELAY_EXT_PIN, LOW);  //init
}

void loop() {
  static int lcdLoop = 0, battery = 0, relay = 0;
  static byte power = 0;
  static String lastLink;
  String result;

  /****** Power status measurement **************************************/
  power = digitizePowerValue(readPowerInput());
  battery = getRealBatteryVoltage() * 1000.0;
  updateLedStatus(LED_PCB_PIN, power == 0);

  if (lcdLoop % LCD_DISPLAY_MOD == 0)
  {
    lcdLoop = 0;
    displayOnLCDX(0, "LoRa Monitor");

    SerialDebug.println("RSSI Reader");
    sendATCommandToLoRa("ATT09\r\n", false, result);
    result.trim();
    displayOnLCDX(2, "RSSI");
    if (result.length() == 7)
    {
      displayOnLCDXY(2, 5, result);
    }
    else
    {
      SerialDebug.println(result);
      displayOnLCDXY(2, 5, "-");
    }

    SerialDebug.println("NETWORK_JOINED");
    sendATCommandToLoRa("ATO201\r\n", false, result);
    result.trim();
    if (result.indexOf("01") >= 0)
    {
      displayOnLCDX(3, "NET_JOINED OK");
    }
    else
    {
      displayOnLCDX(3, "NET_JOINED KO");
    }

    /*    SerialDebug.println("Battery Level");
        sendATCommandToLoRa("ATT08\r\n", false, result);*/
    displayOnLCDX(4, "BAT " + String(battery) + "mv");

    String status = ((power == 0) ? "OFF" : "ON");
    displayOnLCDXY(5, 0, "POW " + status);
    SerialDebug.println("POWER ");
    SerialDebug.println(status);
  }

  /**************** Up Link Data ****************************************/
  /*************** Send Message ? ***************/
  unsigned long millisecondsElapsed = millis();
  if (millisecondsElapsed  >= elapsedTimeSinceLastSending + ULDelayLoop) {

    SerialDebug.println("*** TIME TO SEND DATA... ***");
    //    sendPowerLineValuetoLoRa(power, lastLink);
    sendDatatoLoRa(String(power) + String(relay) + String(battery), lastLink);

    /********* Down Link Get Serial Data *****************************/
    SerialDebug.print("LAST LINK ");
    SerialDebug.println("*" + lastLink + "*");
    if (lastLink.indexOf("OK") >= 0)
    {
      SerialDebug.println("=> UP OK");
      displayOnLCDXY(5, 8, "UP OK");
    }
    else
    {
      SerialDebug.println("=> UP KO");
      displayOnLCDXY(5, 8, "UP KO");
    }

    if (lastLink.indexOf(0xFFFFFFF1) >= 0) // Send F1
    {
      relay = 1;
      SerialDebug.println("Turn Relay ON");
      digitalWrite(RELAY_EXT_PIN, HIGH);
      SerialDebug.println("=> DOWN ON");
      displayOnLCDX(6, "LAST DOWN ON");
    }
    else if (lastLink.indexOf(0xFFFFFFF0) >= 0) // Send F0
    {
      relay = 0;
      SerialDebug.println("Turn Relay OFF");
      digitalWrite(RELAY_EXT_PIN, LOW);
      SerialDebug.println("=> DOWN OFF");
      displayOnLCDX(6, "LAST DOWN OFF");
    }
    else
    {
      SerialDebug.println("No command received...");
      SerialDebug.println("=> NO DOWN DATA");
      displayOnLCDX(6, "NO DOWN DATA");
    }
    elapsedTimeSinceLastSending = millisecondsElapsed;
  }

  unsigned long wait = (elapsedTimeSinceLastSending - millisecondsElapsed + ULDelayLoop) / 1000;
  if (wait <= ULDelayLoop)
  {
    //    SerialDebug.println("Link in " + String(wait) + "s");
    displayOnLCDXY(7, 0, "Link in " + String(wait) + "s   ");
  }

  lcdLoop++;
  delay(1000);
}

void changeRelayState(bool state) {
  digitalWrite(RELAY_EXT_PIN, state);
}

void initLCD() {
  SeeedOled.init();
  SeeedOled.clearDisplay();
  SeeedOled.setNormalDisplay();
  SeeedOled.setPageMode();
}

void displayOnLCDXY(int X, int Y, String string) {
  char lineBuffer[LCD_LINE_NR + 1];
  string.toCharArray(lineBuffer, sizeof(lineBuffer));
  SeeedOled.setTextXY(X, Y);
  SeeedOled.putString(lineBuffer);
}

void displayOnLCDX(int X, String string) {
  char lineBuffer[LCD_LINE_NR + 1];
  memset(lineBuffer, ' ', sizeof(lineBuffer)); lineBuffer[LCD_LINE_NR] = 0;
  displayOnLCDXY(X, 0, String(lineBuffer));
  delay(LCDDelay);
  displayOnLCDXY(X, 0, string);
}

void initLoRaModule() {
  String result;

  // ************ Command Mode *****************
  // set ATO & ATM Parameters (Unconf frame, port com, encoding, rx payload only, Duty Cycle...)
  SerialDebug.println("Enter command mode: +++");
  sendATCommandToLoRa("+++", false, result);
  SerialDebug.println("LoRa Serial rate 19200 ATM007=06");
  sendATCommandToLoRa("ATM007=06\r\n", false, result);
  SerialDebug.println("ATIM Module version & information");
  sendATCommandToLoRa("ATV\r\n", false, result);
  /*      SerialDebug.println("Module Configuration");
        sendATCommandToLoRa("ATM000\r\n", false, result);
        SerialDebug.println("Low Power");
        sendATCommandToLoRa("ATM001\r\n", false, result);
        SerialDebug.println("LED Mode");
        sendATCommandToLoRa("ATM002\r\n", false, result);
        SerialDebug.println("Battery Level");
        sendATCommandToLoRa("ATT08\r\n", false, result);
        SerialDebug.println("Get DevAddr (LSB F) ATO069");
        sendATCommandToLoRa("ATO069\r\n", false, result);*/
  SerialDebug.println("Get DevEUI (LSB F) ATO070");
  sendATCommandToLoRa("ATO070\r\n", false, result);
  SerialDebug.println("Get AppEUI (LSB F) ATO071");
  sendATCommandToLoRa("ATO071\r\n", false, result);
  SerialDebug.println("Get AppKey (LSB F) ATO072");
  sendATCommandToLoRa("ATO072\r\n", false, result);
  /*    SerialDebug.println("Get NwkSKey (LSB F) ATO073");
      sendATCommandToLoRa("ATO073\r\n", false, result);
      SerialDebug.println("Get AppSKey (LSB F) ATO074");
      sendATCommandToLoRa("ATO074\r\n", false, result);
      SerialDebug.println("Spreading");
      sendATCommandToLoRa("ATO075\r\n", false, result);
      SerialDebug.println("Power");
      sendATCommandToLoRa("ATO076\r\n", false, result);
      SerialDebug.println("Channel");
      sendATCommandToLoRa("ATO077\r\n", false, result);
      SerialDebug.println("RSSI Reader");
      sendATCommandToLoRa("ATT09\r\n", false, result);
      SerialDebug.println("Frame Number");
      sendATCommandToLoRa("ATO080\r\n", false, result);
      SerialDebug.println("Wan Command");
      sendATCommandToLoRa("ATO081\r\n", false, result);
      SerialDebug.println("Port Field");
      sendATCommandToLoRa("ATO082\r\n", false, result);
      SerialDebug.println("Frame Size ATO084");
      sendATCommandToLoRa("ATO084\r\n", false, result);*/
  SerialDebug.println("NETWORK_JOINED");
  sendATCommandToLoRa("ATO201\r\n", false, result);

  /***************** JoinMode ****************************************/
  SerialDebug.println("LoRaWan Behaviour ATO083");
  sendATCommandToLoRa("ATO083\r\n", false, result);

#ifdef MODE_OTAA
  if (result[7] != '3' || result[8] != 'F') { // Avoid Restarting if Already in Right Mode
    SerialDebug.println(" --> OTAA ");

    /***************** OTAA ****************************************/
    SerialDebug.println("Set to OTAA mode");
    sendATCommandToLoRa("ATO083=3F\r\n", false, result);
    SerialDebug.println("Save new configuration");
    sendATCommandToLoRa("ATOS", false, result);
    delay(3 * LoRaSerialDelay);
    SerialDebug.println("Restart the module");
    sendATCommandToLoRa("ATR\r\n", false, result);
    delay(3 * LoRaSerialDelay);
  }
#else
  if (result[7] != '3' || result[8] != 'E') { // Avoid Restarting if Already in Right Mode
    SerialDebug.println(" --> ABP ");

    /***************** ABP ****************************************/
    SerialDebug.println("Set to ABP mode");
    sendATCommandToLoRa("ATO083=3E\r\n", false, result);
    SerialDebug.println("Save new configuration");
    sendATCommandToLoRa("ATOS", false, result);
    delay(3 * LoRaSerialDelay);
    SerialDebug.println("Restart the module");
    sendATCommandToLoRa("ATR\r\n", false, result);
    delay(3 * LoRaSerialDelay);
  }
#endif
  /*  SerialDebug.println("Save new configuration");
    sendATCommandToLoRa("ATOS", false, result);
    delay(3 * LoRaSerialDelay);
    SerialDebug.println("Restart the module");
    sendATCommandToLoRa("ATR\r\n", false, result);
    delay(3 * LoRaSerialDelay);*/

  SerialDebug.println("Read configuration from EEPROM");
  sendATCommandToLoRa("ATOR", false, result);
  delay(3 * LoRaSerialDelay);

  SerialDebug.println("Debug Mode ON or OFF");
  sendATCommandToLoRa("ATM17=1\r\n", false, result); // 3: DEBUG MODE ON, 1: DEBUG MODE OFF

  /***********************Quit COMMAND MODE ********************/
  //SerialDebug.println("Quit command mode: ATQ");
  //sendATCommandToLoRa("ATQ\r\r\n", false, result);
}

void sendATCommandToLoRa(String upData, bool extraDelay, String &downData) {
  downData = "";
  int maxIdx = extraDelay ? 3 : 1;

  SerialLoRa.flush();
  SerialLoRa.print(upData);

  for (int idx = 0; idx < maxIdx; idx++)
  {
    unsigned long millisecondsDownLink = millis(); // Current millis()
    while (!SerialLoRa.available())
      if ((millis() - millisecondsDownLink) > ULAirTimeDelay) break;

    downData += SerialLoRa.readString();
  }

  if (downData.length() > 0) {
    if (extraDelay)
    {
      displayStringInHexChar(downData);
    }
    else
    {
      SerialDebug.println(downData);
    }
  }
  delay(LoRaSerialDelay);
}

void blinkLed(int seconds) {
  SerialDebug.print("Led blinking... ");
  SerialDebug.print(seconds);
  SerialDebug.print(" sec");

  int counter = 0;
  while (counter++ < seconds)
  {
    // blink
    digitalWrite(LED_PCB_PIN, HIGH);
    delay(350);

    digitalWrite(LED_PCB_PIN, LOW);
    delay(450);
  }

  SerialDebug.println("... end blinking");
}

void sendPowerLineValuetoLoRa(byte value, String &result) {
  String string = String(value, HEX);

  SerialDebug.print("Send Data: ");
  SerialDebug.println(value, HEX);
  sendATCommandToLoRa("AT$SB=" + string + "\r\n", true, result);
}

void sendDatatoLoRa(String data, String &result) {
  String hexStr = "";
  for (int i = 0; i < data.length(); i++) {
    hexStr += String(data.charAt(i), HEX);
  }

  SerialDebug.print("Send Data: ");
  SerialDebug.println(data + " / " + hexStr);
  sendATCommandToLoRa("AT$SF=" + hexStr + "\r\n", true, result);
}

void updateLedStatus(int led, boolean predicate) {
  if (predicate) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }
}

byte digitizePowerValue(int powerValue) {
  /*  SerialDebug.print("Power Value: ");
    SerialDebug.println(powerValue);*/
  return powerValue > 512 ? 1 : 0;
}

int readPowerInput() {
  int sum = 0, i;
  int battery = getRealBatteryVoltage() * 1000.0; // Measure battery here!
  /*  SerialDebug.print("Battery: ");
    SerialDebug.println(battery);*/

  for (i = 0; i < 3; i++)
  {
    sum += analogRead(PIN_POWERLINE);
  }
  return sum / i;
}

float getRealBatteryVoltage() {
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  return (ADC_AREF / 1023.0) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage;
}

void displayStringInHexChar(String data) {
  SerialDebug.print(data + " ");
  int len = data.length() + 1;
  char *buf = malloc(len);
  data.toCharArray(buf, len);
  for (int i = 0; i < len; i++)
  {
    SerialDebug.print("0x"); SerialDebug.print(buf[i], HEX); SerialDebug.print("/"); SerialDebug.print(buf[i]); SerialDebug.print(" ");
  }
  SerialDebug.println("");
}

