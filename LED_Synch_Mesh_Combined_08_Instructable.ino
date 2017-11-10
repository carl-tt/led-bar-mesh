/*
 *  LED_Synch_Mesh_Send
 *  Each ESP8266 based mesh node is a bar with 30 WS2812 ("Neopixel") LEDs
 *  
 *  The animations are synched between all the nodes
 *  
 *  Libraries used - standing on the back of giants - THANKS!
 *  painlessMesh
 *  FastLED
 *  
 *  2017 Carl F Sutter
 *  
 */

// LED Setup - must come before the painlessMesh so CRGB can be used as a return type
#include <FastLED.h>
// How many leds in your strip?
#define NUM_LEDS 30

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  Note that the GPIO and Data pins are laballed separately on ESP8266 boards
#define DATA_PIN 6

// Define the array of leds
CRGB leds[NUM_LEDS];


// Global Vars and constants
int display_mode = 0;       // this is the current animation type
uint16_t display_step = 0;  // this is a count of the steps in the current animation
CRGB color1;                // used in some animations
bool amController = false;  // flag to designat that this CPU is the current controller
#define MODE6CLICKSPERCOLOR 10   // the number of 50us counts to hold one color
#define MODE6NUMLOOPS 8         // the number of times to change the color

unsigned long ul_PreviousMillis = 0UL;   // using millis rollver code from http://playground.arduino.cc/Code/TimingRollover
unsigned long ul_Interval = 20UL;        // this is delay between animation steps ms


// Mesh Setup
#include "painlessMesh.h"

// set these to whatever you like - don't use your home info - this is a separate private network
#define   MESH_PREFIX     "LEDMesh01"
#define   MESH_PASSWORD   "foofoofoo"
#define   MESH_PORT       5555

void sendMessage(int display_step); // Prototype so PlatformIO doesn't complain

painlessMesh  mesh;
//Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );


// this gets called when the designated controller sends a command to start a new animation
// init any animation specific vars for the new mode, and reset the timer vars
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Setting display mode to %s. Received from %u\n", msg.c_str(), from);

  // get the new display mode
  display_mode = msg.toInt();

  // init some vars that are the same for all animations
  display_step = 0;
  ul_PreviousMillis = 0UL; // make sure animation triggers on first step

  // initialize any variables for the display_mode animation
  switch (display_mode) {
    case 1: // run the single color up the bar one LED at a time
      color1 = CRGB(255, 0, 0);
      ul_Interval = 50UL;
      for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(0, 255, 0); // set all the strips to green
    break;

    case 2: // rainbow colors
      ul_Interval = 50UL;
    break;

    case 3: // go from dim to full on one color for all LEDs a few times
      ul_Interval = 2UL;
    break;

    case 4: // alternate red and green LEDs
      ul_Interval = 50UL;
    break;

    case 5: // animate a dot moving up all of them
      ul_Interval = 50UL;
    break;

    case 6: // green bars with a red bar moving around them a few times
      ul_Interval = 50UL;
    break;

    default:
    break;
  } // display_mode switch    
} // receivedCallback


void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
} // newConnectionCallback


// this gets called when a node is added or removed from the mesh, so set the controller to the node with the lowest chip id
void changedConnectionCallback() {
  SimpleList<uint32_t> nodes;
  uint32_t myNodeID = mesh.getNodeId();
  uint32_t lowestNodeID = myNodeID;

  Serial.printf("Changed connections %s\n",mesh.subConnectionJson().c_str());

  nodes = mesh.getNodeList();
  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  for (SimpleList<uint32_t>::iterator node = nodes.begin(); node != nodes.end(); ++node) {
    Serial.printf(" %u", *node);
    if (*node < lowestNodeID) lowestNodeID = *node;
  }
  Serial.println();

  if (lowestNodeID == myNodeID) {
    Serial.printf("Node %u: I am the controller now", myNodeID);
    Serial.println();
    amController = true;
    // restart the current animation - to chatty - remove - better to wait for next animation
    //sendMessage(display_mode);
    //display_step = 0;
    //ul_PreviousMillis = 0UL; // make sure animation triggers on first step
  } else {
    Serial.printf("Node %u is the controller now", lowestNodeID);
    Serial.println();
    amController = false;
  }
} // changedConnectionCallback

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
} // changedConnectionCallback


void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  
  Serial.begin(115200);

//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  //mesh.scheduler.addTask( taskSendMessage );
  //taskSendMessage.enable() ;

  display_mode = 1;
  sendMessage(display_mode);
  ul_Interval = 50UL;
  color1 = CRGB(255, 0, 0);

  // make this one the controller if there are no others on the mesh
  if (mesh.getNodeList().size() == 0) amController = true;
  
} // setup


void loop() {
  mesh.update();
  stepAnimation();  // update any animation if it's time

  // if the current animation is done, start the next one - only the controller does this, and it messages the rest
  if (amController) {
    bool display_mode_changed = false;
    switch (display_mode) {
      case 1: // run the single color up the bar one LED at a time
        if (display_step > NUM_LEDS) {
          display_mode_changed = true;
          display_mode = 2;  // show all the animations
          //display_mode = 4;  // skip to the holiday only ones
          ul_Interval = 50UL;
        }
        break;

      case 2: // rainbow colors 
        if (display_step > 255) {
          display_mode_changed = true;
          display_mode = 3;
          ul_Interval = 2UL;
        }
        break;

      case 3: // go from dim to full on one color for all LEDs a few times
        if (display_step > 1024) {
          display_mode_changed = true;
          display_mode = 4;
          ul_Interval = 50UL;
        }
        break;

      case 4: // alternate red and green LEDs
        if (display_step > (NUM_LEDS*4)) {
          display_mode_changed = true;
          display_mode = 5;
          ul_Interval = 50UL;
        }
        break;

      case 5: // animate a dot moving up all of them in order
        if (display_step >= ((mesh.getNodeList().size() + 1) * NUM_LEDS)) {
          display_mode_changed = true;
          display_mode = 6;
          ul_Interval = 50UL;
        }
        break;

      case 6: // green bars with a red bar moving around them a few times
        if (display_step >= ((mesh.getNodeList().size() + 1) * MODE6CLICKSPERCOLOR * MODE6NUMLOOPS)) {  // 10 steps per color (x50us), 8 color changes.  total run time is 8*(#strips/2)
          display_mode_changed = true;
          display_mode = 1;
          ul_Interval = 50UL;
        }
        break;
     } // display_mode switch

     // if the display mode changed, do some common tasks and set some common vars
     if (display_mode_changed) {
       sendMessage(display_mode);
       display_step = 0;
       ul_PreviousMillis = 0UL; // make sure animation triggers on first step
     }
  } // amController

} // loop


// sort the given list of nodes
void sortNodeList(SimpleList<uint32_t> &nodes) {
  SimpleList<uint32_t> nodes_sorted;
  SimpleList<uint32_t>::iterator smallest_node;

  // sort the node list
  while (nodes.size() > 0) {
    // find the smallest one
    smallest_node = nodes.begin();
    for (SimpleList<uint32_t>::iterator node = nodes.begin(); node != nodes.end(); ++node) {
      if (*node < *smallest_node) smallest_node = node;
    }
    
    // add it to the sorted list and remove it from the old list
    nodes_sorted.push_back(*smallest_node);
    nodes.erase(smallest_node);
  } // while
  
  // copy the sorted list back into the now empty nodes list
  for (SimpleList<uint32_t>::iterator node = nodes_sorted.begin(); node != nodes_sorted.end(); ++node) nodes.push_back(*node);
} // sortNodeList


// check the timer and do one animation step if needed
void stepAnimation() {
  // if the animation delay time has past, run another animation step
  unsigned long ul_CurrentMillis = millis();  
  if (ul_CurrentMillis - ul_PreviousMillis > ul_Interval) {
    ul_PreviousMillis = millis();
    switch (display_mode) {
      case 1: // run the single color up the bar one LED at a time
        leds[display_step % NUM_LEDS] = color1;
        FastLED.show();
      break;

      case 2: // rainbow colors
        leds[display_step % NUM_LEDS] = Wheel(display_step % 255);
        // we could also set all the pixels at once
        //for(uint16_t i=0; i< strip.numPixels(); i++) {
        //  strip.setPixelColor(i, Wheel((i+display_step) & 255));
        //  //strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + display_step) & 255));
        //}
        FastLED.show();
      break;

      case 3: // go from dim to full on one color for all LEDs a few times
        for(uint16_t i=0; i<NUM_LEDS; i++) {
          leds[i] = CRGB(0, 0, display_step % 255);
        }
        FastLED.show();
      break;

      case 4: // alternate red and green LEDs
        if (((display_step + (display_step / NUM_LEDS)) % 2) == 0) {
          leds[display_step % NUM_LEDS] = CRGB(64, 0, 0);
        } else {
          leds[display_step % NUM_LEDS] = CRGB(0, 64, 0);
        }
        FastLED.show();
      break;

      case 5: // animate a dot moving up all of them 
      {
        int curr_strip_num = display_step / NUM_LEDS;
        SimpleList<uint32_t> nodes = mesh.getNodeList();
        int node_index = 0;
        uint32_t myNodeID = mesh.getNodeId();

        // add this node to the node list and sort the list
        nodes.push_back(myNodeID);
        sortNodeList(nodes);
        
        // blacken all the leds
        for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0);

        // if this is the active strip, set the next LED on
        SimpleList<uint32_t>::iterator node = nodes.begin();
        while (node != nodes.end()) {
          if ((*node == myNodeID) && (node_index == curr_strip_num)) {
            leds[display_step % NUM_LEDS] = CRGB(255, 255, 255);
          }
          node_index++;
          node++;
        }
        FastLED.show();
      }
      break;

      case 6: // green bars with a red bar moving around them a few times
      {
        SimpleList<uint32_t> nodes = mesh.getNodeList();
        int curr_strip_num = (display_step / MODE6CLICKSPERCOLOR) % (nodes.size() + 1);
        int node_index = 0;
        uint32_t myNodeID = mesh.getNodeId();

        // add this node to the node list and sort the list
        nodes.push_back(myNodeID);
        sortNodeList(nodes);

        // set all the leds to green
        for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(0, 255, 0);

        // if this is the active strip, set the next LED on
        SimpleList<uint32_t>::iterator node = nodes.begin();
        while (node != nodes.end()) {
          if ((*node == myNodeID) && (node_index == curr_strip_num)) {
            for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(255, 0, 0);
          }
          node_index++;
          node++;
        }
        FastLED.show();
      }
      break;
      
    } // display_mode switch

    display_step++;
    // a debug led to show which is the current controller
    //if (amController) leds[0] = CRGB(64, 64, 64);
  } // timing conditional ul_Interval
} // stepAnimation


// send a broadcast message to all the nodes specifying the new animation mode for all of them
void sendMessage(int display_step) {
  String msg;
  //msg += mesh.getNodeId();
  msg += String(display_step);
  mesh.sendBroadcast( msg );
} // sendMessage


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
CRGB Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
}

