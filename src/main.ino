#include <Arduino.h>
#include <ArduinoBleOTA.h>
#include <ArduinoBleChess.h>
#include <BleChessUuids.h>
#include <BleOtaUuids.h>
#include <BleChessMultiservice.h>
#include "openchessboard.h"

#define DEVICE_NAME "OCHESSBOARD" // max name size with 128 bit uuid is 11
#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial

bool skip_next_send = false;
bool my_castling_rights = true;
bool opp_castling_rights = true;

bool game_running = false;

bool checkCastling(String move_input) {
  //check if last move was king move from castling
  if(((move_input == "e1g1") || (move_input == "e1c1") ||  (move_input == "e8g8") || (move_input == "e8c8")) ){
    
    if (my_castling_rights)
    {
      my_castling_rights = false;
      DEBUG_SERIAL.println("my castle move...");
      return true;
    }
    if (opp_castling_rights)
    {
      opp_castling_rights = false;
      DEBUG_SERIAL.print("opponent castle move...");
      return true;
    }
  }
  return false;
}

class Peripheral : public BleChessPeripheral
{
public:
  void onFeature(const String& feature) override {
    const bool isSuppported =
      feature == "msg" ||
      feature == "last_move";
    sendAck(isSuppported);
  }

  void onFen(const String& fen) override {
    clearDisplay();
    displayWtoPlay();
    
    game_running = true;
    DEBUG_SERIAL.print("new game: ");
    DEBUG_SERIAL.println(fen.c_str());

    sendAck(true);
  }

  void onMove(const String& mv) override {
    clearDisplay();
    if (game_running){
      DEBUG_SERIAL.print("moved from central: ");
      DEBUG_SERIAL.println(mv.c_str());
      displayMove(mv.c_str());
      skip_next_send = true;
    }

    sendAck(true);
  }

  void onMoveAck(bool ack) override {
    if (ack){
      onMoveAccepted();
    }
    else{
      onMoveRejected();
    }
  }

  void onPromote(const String& mv) override {
    DEBUG_SERIAL.print("promoted on central screen: ");
    DEBUG_SERIAL.println(mv.c_str());

    sendAck(true);
  }

  void onLastMove(const String& mv) override {
    DEBUG_SERIAL.print("last move: ");
    DEBUG_SERIAL.println(mv.c_str());

    sendAck(true);
  }

  void onMoveAccepted(){
    if (game_running){
      DEBUG_SERIAL.print("move accepted: ");
      DEBUG_SERIAL.println(lastPeripheralMove.c_str());
      clearDisplay();
      // game_running = false;
      skip_next_send = false;
      // my_castling_rights = true;
      // opp_castling_rights = true;
    }
  }

  void onMoveRejected() {
    if (game_running){
      DEBUG_SERIAL.print("move rejected: ");
      DEBUG_SERIAL.println(lastPeripheralMove.c_str());
      for (int k = 0; k < 3; k++){
        clearDisplay();
        delay(200);
        displayMove(lastPeripheralMove.c_str());
        delay(200); 
      }
      skip_next_send = true; /* give one try to reverse move, before sending next move to central */
    }
  }
  
  void checkPeripheralMove() {

      String move;
      move = getMoveInput();

      if(checkCastling(move)){
        getMoveInput(); /*get second move from castling but do not send it: send king move only after second input */
      }

      DEBUG_SERIAL.print("moved from peripheral: ");
      DEBUG_SERIAL.println(move.c_str());
      
      clearDisplay();
      if (!skip_next_send){
        sendMove(move);
        lastPeripheralMove = move;
      }
      skip_next_send = false; /*skip only once, then allow new move send */
  }

private:
  String lastPeripheralMove;
};
Peripheral peripheral{};


void setup() {
  initHW();

  DEBUG_SERIAL.begin(115200);
#if DEBUG
  while(!Serial);
#endif
  DEBUG_SERIAL.println("BLE init: OPENCHESSBOARD");
  initBle(DEVICE_NAME);
  if (!ArduinoBleChess.begin("Chess board", peripheral)){
    DEBUG_SERIAL.println("Ble chess initialization error");
  }
  if (!ArduinoBleOTA.begin(InternalStorage)) {
    DEBUG_SERIAL.println("Ble ota initialization error");
  }
  advertiseBle(DEVICE_NAME, BLE_CHESS_SERVICE_UUID, BLE_OTA_SERVICE_UUID);
  DEBUG_SERIAL.println("start BLE polling...");

}


void loop() {
  BLE.poll();
  ArduinoBleOTA.pull();
  peripheral.checkPeripheralMove();
}
