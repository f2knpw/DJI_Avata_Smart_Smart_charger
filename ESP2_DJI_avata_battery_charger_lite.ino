// Copyright freedom2000 2023
// most of DJI protocol was explained by AirCruiser on Hackaday's project : https://hackaday.io/project/188194-dji-fpv-battery-breakout-mod-2-communication
// this project is detailled on hackaday : https://hackaday.io/project/189188-dji-avata-smart-smart-charger
//Licenses
//Unless otherwise stated, all software/code-based works presented here are subject to the GNU General Public License v3.0 : https://www.gnu.org/licenses/gpl-3.0.en.html
/*
 * 1. Anyone can copy, modify and distribute this software.
2. You have to include the license and copyright notice with each and every distribution.
3. You can use this software privately.
4. You can use this software for commercial purposes.
5. If you dare build your business solely from this code, you risk open-sourcing the whole code base.
6. If you modify it, you have to indicate changes made to the code.
7. Any modifications of this code base MUST be distributed with the same license, GPLv3.
8. This software is provided without warranty.
9. The software author or license can not be held liable for any damages inflicted by the software.
 */


//DJI serial stuff
#define RX1 12
#define TX1 14
#define RX2 27
#define TX2 26

//DJI DUML variables
int duml[256];  //hold DUML received bytes
long lastFrame, framePeriod;
int data[150]; //hold the payload part of the DUML message
int idx = 0;
int packetLength, protocolVersion, senderId, senderType, receiverId, receiverType, seqNum, cmdType, ack, commandSet, commandId;
int CRC8, packetCRC16[2], calcPacketCRC16[2] ;
int voltage, current, designCapacity, remainCapacity, temperature, nbCells, percentage, maxV ;
int vC1, vC2, vC3, vC4;
int stopChargePercentage = 60;
boolean endCharge = false;
long chargerTimeOut = 500;
boolean chargerOk = false;


//Preferences
#include <Preferences.h>
Preferences preferences;



long LastBLEnotification;

void setup()
{
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
  // Serial2.begin(115200, SERIAL_8N1, RX2, TX2);



  //Preferences
  preferences.begin("Avata", false);

  //preferences.clear();              // Remove all preferences under the opened namespace
  //preferences.remove("counter");   // remove the counter key only
  stopChargePercentage = preferences.getInt("EoC", 60);       //default is 80% End of Charge

  //preferences.end();  // Close the Preferences

  Serial.println("*********************************************************");
  Serial.println("waiting for serial2 reception");
  Serial.print ( "sendT");
  Serial.print("\t");
  Serial.print ( "recT");
  Serial.print("\t");
  Serial.print ( "seqNum");
  Serial.print("\t");
  Serial.print ( "cmdT");
  Serial.print("\t");
  Serial.print ( "cmdSet");
  Serial.print("\t");
  Serial.println ( "cmdId");
  lastFrame = millis();
  framePeriod = 160;

}




void loop()
{
  while (Serial1.available())
  {
    int c = Serial1.read();
    if (c == 0x55) //start of DUML frame
    {
      if (idx > 13) //we have received a long enough packet ==> decode it
      {
        packetLength =  ((duml[2] << 8) | (duml[1] & 255)) & 1023;
        protocolVersion =  (duml[2] & 252) >> 2;
        CRC8 = duml[3];
        senderId = duml[4] >> 5;
        senderType = duml[4] & 31;  //5 charger, 11 battery
        receiverId = duml[5] >> 5;
        receiverType = duml[5] & 31;
        seqNum = (duml[8] << 8) | (duml[7] & 255);
        cmdType = duml[8] >> 7; // 0 request, 1 answer
        ack = (duml[8] >> 4) & 7;
        commandSet = duml[9]; //13 smart Battery
        commandId = duml[10];
        //Smart battery [CmdSet 13]
        //GetStatic [CmdID 1]
        //GetPushDynamicData [CmdID 2]
        //GetPushCellVoltage [CmdID 3]
        //GetBarCode [CmdID  4]
        //Unknown [CmdID 23]
        //Battery Ping 1 (???) [CmdID 25]
        //Authentication [CmdID 35]
        //Battery Ping 2 (???) [CmdID 67]
        packetCRC16[0] = duml[idx - 2];
        packetCRC16[1] = duml[idx - 1];
        calcCRC16( duml, calcPacketCRC16) ;
        for (int ii = 0; ii < 150; ii++) data[ii] = 8;
        for (int ii = 11; ii < idx - 2; ii++) data[ii - 11] = duml[ii]; //extract the payload from DUML

        if ((packetCRC16[0] == calcPacketCRC16[0]) && (packetCRC16[1] == calcPacketCRC16[1])) //good CRC16
        {
          //        Serial.print (packetLength);
          //        Serial.print("\t");
          Serial.print ( senderType);
          Serial.print("\t");
          Serial.print ( receiverType);
          Serial.print("\t");
          Serial.print ( seqNum);
          Serial.print("\t");
          Serial.print ( cmdType);
          Serial.print("\t");
          Serial.print ( commandSet);
          Serial.print("\t");
          Serial.print ( commandId);
          Serial.print("\t");
          switch (senderType)
          {
            case 5: //charger speaking
              framePeriod = (31 * framePeriod + (millis() - lastFrame)) / 32;
              //framePeriod = (millis()-lastFrame);
              lastFrame = millis();
              chargerOk = true;
              //              Serial.print("framePeriod ");
              //              Serial.print(framePeriod);
              break;

            case 11:  //battery speaking
              switch (commandId)
              {
                case 1:
                  Serial.print("GetStatic");
                  break;
                case 2:
                  Serial.print("GetPushDynamicData");
                  voltage = (data[3] << 8) | (data[2] & 255);
                  current = (data[7] << 8) | (data[6] & 255);
                  designCapacity = (data[11] << 8) | (data[10] & 255);
                  remainCapacity = (data[15] << 8) | (data[14] & 255);
                  temperature = (data[19] << 8) | (data[18] & 255);;
                  nbCells = data[20];
                  percentage = (data[22] << 8) | (data[21] & 255);
                  maxV = (data[32] << 8) | (data[31] & 255);
                  Serial.print("\t");
                  Serial.print (voltage);
                  Serial.print("\t");
                  Serial.print (current);
                  Serial.print("\t");
                  Serial.print (designCapacity);
                  Serial.print("\t");
                  Serial.print (remainCapacity);
                  Serial.print("\t");
                  Serial.print (temperature);
                  Serial.print("\t");
                  Serial.print (nbCells);
                  Serial.print("\t");
                  Serial.print (percentage);
                  //                Serial.print("\t");
                  //                Serial.print (maxV);
                  //
                  for (int i = 0; i < 80; i++)
                  {
                    Serial.print("\t");
                    Serial.printf ("%x", data[i]);
                  }
                  break;
                case 3:
                  Serial.print("GetPushCellVoltage (mV)");
                  vC1 = (data[4] << 8) | (data[3] & 255); //voltage_Cell_1 mV
                  vC2 = (data[6] << 8) | (data[5] & 255);//voltage_Cell_2 mV
                  vC3 = (data[8] << 8) | (data[7] & 255);//voltage_Cell_3 mV
                  vC4 = (data[10] << 8) | (data[9] & 255);//voltage_Cell_4 mV
                  Serial.print("\t");
                  Serial.print (vC1);
                  Serial.print("\t");
                  Serial.print (vC2);
                  Serial.print("\t");
                  Serial.print (vC3);
                  Serial.print("\t");
                  Serial.print (vC4);
                  break;
                case 4:
                  Serial.print("GetBarcode");

                  break;
                default:
                  break;
              }
              break;
            default:
              break;
          }
          //        Serial.print("\t");
          //        Serial.print (CRC16);
          //        Serial.print("\t");
          //        Serial.print (calcCRC8(duml));
          //        Serial.print("\t");
          //        Serial.print (packetCRC16[0]);
          //        Serial.print("\t");
          //        Serial.print (packetCRC16[1]);
          //        Serial.print("\t");
          //        Serial.print (calcPacketCRC16[0]);
          //        Serial.print("\t");
          //        Serial.println (calcPacketCRC16[1]);
          Serial.println (" ");

        }
      }
      else Serial.println("bad CRC16"); //skip frame
      idx = 0;                          //start new frame
    }
    //Serial.printf("%x", c);
    duml[idx] = c;
    idx++;
  }
  //charger status
  if ((millis() - lastFrame) > chargerTimeOut) chargerOk = false;

  //send BLE notification
  if (((millis() - LastBLEnotification) > 5000) && (voltage > 10) && (endCharge == false)) // send BLE message to Android App (no need to be connected, a simple notification, send and forget)
  {
    if (percentage >= stopChargePercentage)    //handle end of charge condition
    {
      pinMode(RX1, OUTPUT);
      digitalWrite(RX1, LOW);                 //will pulldown the RX1 pin and stop charge/battery handshake
      endCharge = true;
      chargerOk = false;
      Serial.print("Stop charge as above ");
      Serial.print(stopChargePercentage);
      Serial.println(" %");
    }
    LastBLEnotification = millis();
    String res;
    res = "{\"V\":\"" + String(voltage) + "\",\"I\": " + String(current) + ",\"Dc\": " + String(designCapacity) + ",\"Rc\": " + String(remainCapacity) ;
    res += + ",\"T\": " + String( temperature) + ",\"Per\": " + String( percentage) + ",\"Vc1\": " + String(vC1) + ",\"Vc2\": " + String(vC2) + ",\"Vc3\": " + String(vC3) + ",\"Vc4\": " + String(vC4) ;
    res += + ",\"St\": " + String( chargerOk) + ",\"Cut\": " + String( stopChargePercentage) + "}";
    Serial.println(res);
    
  }
}

int calcCRC8(int buff[])
{
  int value     = 119;
  int crc8      = 0;
  int LUTCRC8[] = {0, 94, -68, -30, 97, 63, -35, -125, -62, -100, 126, 32, -93, -3, 31, 65, -99, -61, 33, 127, -4, -94, 64, 30, 95, 1, -29, -67, 62, 96, -126, -36, 35, 125, -97, -63, 66, 28, -2, -96, -31, -65, 93, 3, -128, -34, 60, 98, -66, -32, 2, 92, -33, -127, 99, 61, 124, 34, -64, -98, 29, 67, -95, -1, 70, 24, -6, -92, 39, 121, -101, -59, -124, -38, 56, 102, -27, -69, 89, 7, -37, -123, 103, 57, -70, -28, 6, 88, 25, 71, -91, -5, 120, 38, -60, -102, 101, 59, -39, -121, 4, 90, -72, -26, -89, -7, 27, 69, -58, -104, 122, 36, -8, -90, 68, 26, -103, -57, 37, 123, 58, 100, -122, -40, 91, 5, -25, -71, -116, -46, 48, 110, -19, -77, 81, 15, 78, 16, -14, -84, 47, 113, -109, -51, 17, 79, -83, -13, 112, 46, -52, -110, -45, -115, 111, 49, -78, -20, 14, 80, -81, -15, 19, 77, -50, -112, 114, 44, 109, 51, -47, -113, 12, 82, -80, -18, 50, 108, -114, -48, 83, 13, -17, -79, -16, -82, 76, 18, -111, -49, 45, 115, -54, -108, 118, 40, -85, -11, 23, 73, 8, 86, -76, -22, 105, 55, -43, -117, 87, 9, -21, -75, 54, 104, -118, -44, -107, -53, 41, 119, -12, -86, 72, 22, -23, -73, 85, 11, -120, -42, 52, 106, 43, 117, -105, -55, 74, 20, -10, -88, 116, 42, -56, -106, 21, 75, -87, -9, -74, -24, 10, 84, -41, -119, 107, 53};
  for (int i = 0; i < 3; i++) {
    value = LUTCRC8[(value ^ buff[i]) & 255];
  }
  crc8 = value & 0xff;
  return crc8;
}
void calcCRC16(int buff[], int crc16[]) {
  int value      = 13970;
  int LUTCRC16[] = {0, 4489, 8978, 12955, 17956, 22445, 25910, 29887, 35912, 40385, 44890, 48851, 51820, 56293, 59774, 63735, 4225, 264, 13203, 8730, 22181, 18220, 30135, 25662, 40137, 36160, 49115, 44626, 56045, 52068, 63999, 59510, 8450, 12427, 528, 5017, 26406, 30383, 17460, 21949, 44362, 48323, 36440, 40913, 60270, 64231, 51324, 55797, 12675, 8202, 4753, 792, 30631, 26158, 21685, 17724, 48587, 44098, 40665, 36688, 64495, 60006, 55549, 51572, 16900, 21389, 24854, 28831, 1056, 5545, 10034, 14011, 52812, 57285, 60766, 64727, 34920, 39393, 43898, 47859, 21125, 17164, 29079, 24606, 5281, 1320, 14259, 9786, 57037, 53060, 64991, 60502, 39145, 35168, 48123, 43634, 25350, 29327, 16404, 20893, 9506, 13483, 1584, 6073, 61262, 65223, 52316, 56789, 43370, 47331, 35448, 39921, 29575, 25102, 20629, 16668, 13731, 9258, 5809, 1848, 65487, 60998, 56541, 52564, 47595, 43106, 39673, 35696, 33800, 38273, 42778, 46739, 49708, 54181, 57662, 61623, 2112, 6601, 11090, 15067, 20068, 24557, 28022, 31999, 38025, 34048, 47003, 42514, 53933, 49956, 61887, 57398, 6337, 2376, 15315, 10842, 24293, 20332, 32247, 27774, 42250, 46211, 34328, 38801, 58158, 62119, 49212, 53685, 10562, 14539, 2640, 7129, 28518, 32495, 19572, 24061, 46475, 41986, 38553, 34576, 62383, 57894, 53437, 49460, 14787, 10314, 6865, 2904, 32743, 28270, 23797, 19836, 50700, 55173, 58654, 62615, 32808, 37281, 41786, 45747, 19012, 23501, 26966, 30943, 3168, 7657, 12146, 16123, 54925, 50948, 62879, 58390, 37033, 33056, 46011, 41522, 23237, 19276, 31191, 26718, 7393, 3432, 16371, 11898, 59150, 63111, 50204, 54677, 41258, 45219, 33336, 37809, 27462, 31439, 18516, 23005, 11618, 15595, 3696, 8185, 63375, 58886, 54429, 50452, 45483, 40994, 37561, 33584, 31687, 27214, 22741, 18780, 15843, 11370, 7921, 3960};
  for (int i = 0; i < idx - 2; i++) {
    value = LUTCRC16[(value ^ buff[i]) & 255] ^ (value >> 8);
  }
  crc16[0] = value & 255;
  crc16[1] = (65280 & value) >> 8;
}
