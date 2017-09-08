// twister.ino
//
// This is Arduino sketch for Artificial Muscles Twister (AMT).
// AMT - A device for producing artificial muscles from nylon 
// fishing line with a heater wire.
// Paper: A. N. Semochkin, "A device for producing artificial 
// muscles from nylon fishing line with a heater wire", 
// 2016 IEEE International Symposium on Assembly and Manufacturing (ISAM), 
// Fort Worth, TX, USA, 2016, pp. 26-30. doi: 10.1109/ISAM.2016.7750715.
// The device has two general operations: winding the wire on the polymer fiber 
// and twisting the fiber into a helical spring. All functions are available 
// through the control panel.
// 
// The description, software, 3D models and assembly guide are available 
// as open source : http://iskanderus.ru/amt-core and
// https://www.wevolver.com/alexander.semochkin/artificial-muscle-twister/main/description
//
// This is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// Copyright (C) 2015-2017 Aleksandr Semochkin
// Contacts:
// https://www.facebook.com/iskanderuse
// http://iskanderus.ru
// https://www.youtube.com/user/Iskanderuse
// http://lit.bgpu.ru

#include <EEPROM2.h>
#include <AccelStepper.h>
#include <LiquidCrystal.h>
#include <Rotary.h>
//======= LCD ========================================================
#define LCD_SPEAKER 37

#define LCD_RS     16
#define LCD_Enable 17
#define LCD_D4     23
#define LCD_D5     25
#define LCD_D6     27
#define LCD_D7     29
LiquidCrystal lcd(LCD_RS, LCD_Enable, LCD_D4, LCD_D5, LCD_D6,LCD_D7);
//======= ROTARY======================================================
#define ROTARY_EN1    31
#define ROTARY_EN2    33
#define ROTARY_BUTTON 35
Rotary rotary = Rotary(ROTARY_EN1, ROTARY_EN2);
//======= ENDSTOPS ===================================================
#define END_STOP_1 3
#define END_STOP_2 2
#define END_STOP_3 14
#define END_STOP_4 15
//======= STEPPERS ===================================================
#define USEDRIVER 1
//---- Stepper Motor 1 (RAMPS X)-------------------------------------
#define SM_1_STEP_PIN       54
#define SM_1_DIR_PIN        55
#define SM_1_ENABLE_PIN     38
AccelStepper SM_1(USEDRIVER, SM_1_STEP_PIN, SM_1_DIR_PIN); 
//---- Stepper Motor 2  (RAMPS Y)-------------------------------------
#define SM_2_STEP_PIN       60
#define SM_2_DIR_PIN        61
#define SM_2_ENABLE_PIN     56
AccelStepper SM_2(USEDRIVER, SM_2_STEP_PIN, SM_2_DIR_PIN); 
//---- Stepper Motor 3  (RAMPS E0)-------------------------------------
#define SM_3_STEP_PIN       26
#define SM_3_DIR_PIN        28
#define SM_3_ENABLE_PIN     24
AccelStepper SM_3(USEDRIVER, SM_3_STEP_PIN, SM_3_DIR_PIN); 
//---- Stepper Motor 4  (RAMPS E1)-------------------------------------
#define SM_4_STEP_PIN       36
#define SM_4_DIR_PIN        34
#define SM_4_ENABLE_PIN     30
AccelStepper SM_4(USEDRIVER, SM_4_STEP_PIN, SM_4_DIR_PIN); 

void initStepper(void *s,int ENABLE_PIN)
{ AccelStepper *stepper=(AccelStepper *)s;
  //stepper->setMaxSpeed(200*16*15000);
  stepper->setMaxSpeed(100*16);
  stepper->setAcceleration(200);
  stepper->setMinPulseWidth(20);
  stepper->setCurrentPosition(0);

  stepper->setEnablePin(ENABLE_PIN); 
  stepper->setPinsInverted(false, false, true); //invert logic of enable pin
  stepper->enableOutputs();
  stepper->stop();
}

void setStretcherStepper(void *s)
{ AccelStepper *stepper=(AccelStepper *)s;
  stepper->setMaxSpeed(100*16);
  stepper->setAcceleration(200);
}

void setTwisterStepper(void *s,float maxspeed,float acccel)
{ AccelStepper *stepper=(AccelStepper *)s;
  stepper->setMaxSpeed(maxspeed);
  stepper->setAcceleration(acccel);
}

//------------------------------------------------------------------------
long    twist_counter=0l;
//----- Menu ----------------------------------------------------------
#define JUST_MENU         (byte)255
#define INFO_MENU         (byte)1
#define FIRST_1           (byte)2
#define FIRST_2           (byte)3
#define HOME_MENU         (byte)4
#define PREPARE_MENU      (byte)5
#define SAVE_MENU         (byte)6
#define SAVE_TWIST        (byte)7
#define STRETCH_MENU      (byte)8
#define REEL_HOME_MENU    (byte)9
#define REEL_START_MENU   (byte)10
#define REEL_RIGHT_MENU   (byte)11
#define TWIST_MENU        (byte)12
#define COIL_MENU         (byte)13
#define PLAY_MENU         (byte)14
#define SHOW_MEM_MENU     (byte)15
#define COIL_DIR_MENU     (byte)16

byte  current_state=JUST_MENU;

const char *logo="Iskanderus AMTwister";
const char *push_string="Push to select..";
const char *interrupt_string="Push to interrupt..";

#define ROTARY_RIGHT   16
#define ROTARY_LEFT    32
#define ROTARY_CONFIRM 64
byte leftrow[8] = {
  B00000,
  B00100,
  B01100,
  B11111,
  B01100,
  B00100,
  B00000,
};
byte rightrow[8] = {
  B00000,
  B00100,
  B00110,
  B11111,
  B00110,
  B00100,
  B00000,
};

class Menu
{ 
public:  
  const char* mname;
  const char**list;
  const char**helps;
  byte *states;
  byte  menu_length=0;
  byte  menu_index = 0;
  Menu  **menus;

public:  
  Menu(const char *n,const char**li,const char**he,byte *s,int len)
  { mname=n;
    menu_length=len;     list=li;    helps=he;    menu_index = 0; 
    states=s;
  }
  
  void show()
  { menu_index = 0;    
    printMenu();
  }
  
  void setChildren(Menu  **m)
  {menus=m;
  }
  byte rotary_changed(int value); 
  
  void printMenu() 
  { lcd.clear();
    lcd.print(logo);
    lcd.setCursor(0,1);
      lcd.print(mname);
    lcd.setCursor(0,2);
    if(menu_index>0) lcd.write(byte(0));
    lcd.print(" ");
    lcd.print(list[menu_index]);
    lcd.print(" ");
    if(menu_index<menu_length-1) lcd.write(byte(1)); 
    lcd.setCursor(0,3);
    //lcd.print(current_state);
    lcd.print(helps[menu_index]);
  }
  
};

Menu *menu=NULL;
const char* list_main[]  = {"Info",    "Settings",  "Operate"};
const char* helps_main[] = {push_string,push_string,push_string};
byte  states_main[]= {INFO_MENU,  JUST_MENU,  JUST_MENU};

const char* list_settings[] = {"Prepare","Save prepared", "Save twist mem", "Show twist mem","Coil direction","Exit"};
const char* helps_settings[] = {push_string,push_string,push_string,push_string,push_string,push_string};
byte  states_settings[]= {PREPARE_MENU,SAVE_MENU,SAVE_TWIST,SHOW_MEM_MENU,COIL_DIR_MENU,JUST_MENU};

const char* list_operate[] = {"Home", "Stretch","Reel","Twist","Exit"};
const char* helps_operate[] = {push_string,push_string,push_string};
byte  states_operate[]= {HOME_MENU,STRETCH_MENU,JUST_MENU,JUST_MENU,JUST_MENU};

const char* list_reel[] = {"Home", "Start/Continue","Move right","Exit"};
const char* helps_reel[] = {push_string,push_string,push_string,push_string};
byte  states_reel[]= {REEL_HOME_MENU,REEL_START_MENU,REEL_RIGHT_MENU,JUST_MENU};

const char* list_twist[] =  {"Start twisting", "Start coiling","Play memory","Exit"};
const char* helps_twist[] = {push_string,push_string,push_string,push_string};
byte  states_twist[]= {TWIST_MENU,COIL_MENU,PLAY_MENU,JUST_MENU};


Menu mainmenu("main",list_main,helps_main,states_main,3);
Menu settingsmenu(list_main[1],list_settings,helps_settings,states_settings,6);
Menu operatemenu(list_main[2],list_operate,helps_operate,states_operate,5);
Menu reelmenu(list_operate[2],list_reel,helps_reel,states_reel,4);
Menu twistmenu(list_operate[3],list_twist,helps_twist,states_twist,4);



Menu  *menus_main[] ={NULL,&settingsmenu,&operatemenu};
Menu  *menus_settings[] ={NULL,NULL,NULL,NULL,NULL,&mainmenu};
Menu  *menus_operate[] ={NULL,NULL,&reelmenu,&twistmenu,&mainmenu};
Menu  *menus_reel[] ={NULL,NULL,NULL,&operatemenu};
Menu  *menus_twist[] ={NULL,NULL,NULL,&operatemenu};

byte Menu::rotary_changed(int value) 
  {  if(value == ROTARY_LEFT)
     {  if(menu_index > 0 )
        { --menu_index;
        }
        printMenu();
     } 
     else 
     if (value == ROTARY_RIGHT && menu_index < menu_length-1) 
     {  menu_index++;
        printMenu();
     } 
     else
     if (value == ROTARY_CONFIRM) 
     {      if(menus[menu_index]!=NULL)
            { menu=menus[menu_index];
            }
            return states[menu_index];
     }
     return JUST_MENU;
  }
//---------------------------------------------------------------------  
void menuSetup()
  { lcd.createChar(0, leftrow);
    lcd.createChar(1, rightrow); 
    menu=&mainmenu;
    mainmenu.setChildren(menus_main);
    settingsmenu.setChildren(menus_settings);
    operatemenu.setChildren(menus_operate);
    reelmenu.setChildren(menus_reel);
    twistmenu.setChildren(menus_twist);
    menu->printMenu();
  }
//---------------------------------------------------------------------  
void backToMenu(void *s)
{  menu=(Menu *)s;
   current_state=JUST_MENU;
   menu->show();
}
//--------------------------------------------------------------------
void showMessage(const char *mes,const char *help)
{  lcd.clear();lcd.print(logo);
   lcd.setCursor(0,1);lcd.print(mes);
   lcd.setCursor(0,2);lcd.print(help);
}
void showCoilDirection(const char *mes,const char *help, byte arrow)
{  lcd.clear();lcd.print(logo);
   lcd.setCursor(0,1);lcd.print("Coiling direction");
   lcd.setCursor(0,2);
   if(arrow==0) { lcd.write(byte(arrow)); lcd.print(" ");}
   else lcd.print("  ");
   lcd.print(mes);
   if(arrow==1) {lcd.print(" ");lcd.write(byte(arrow)); }
   lcd.setCursor(0,3);lcd.print(help);
}
void showBounds(int line,const char *mes,long a,long b)
{  char val_1[20];
   char val_2[20];
   lcd.setCursor(0,line);
   lcd.print(mes);
   lcd.print("[");
   lcd.print(ltoa(a,val_1,10));
   lcd.print(",");
   lcd.print(ltoa(b,val_2,10));
   lcd.print("]");
}
void showTwistMemory(int line,int index,int max_index,float mspeed,unsigned long mtime)
{  char val_1[20];
   lcd.clear();lcd.print(logo);
   lcd.setCursor(0,line);
   lcd.print("Index=");
   lcd.print(ltoa(index,val_1,10));
   lcd.print(" Len=");
   lcd.print(ltoa(max_index,val_1,10));
   lcd.setCursor(0,line+1);
   lcd.print("Speed=");
   //sprintf(val_1,"%f", mspeed);//doesn't work
   dtostrf(mspeed, 8,2, val_1);
   lcd.print(val_1);
   lcd.setCursor(0,line+2);
   lcd.print("Time=");
   sprintf(val_1, "%lu", mtime);
   lcd.print(val_1);
}

///////////////////////////////////////////////////////////////////////
unsigned long lastTime;

long  SM3_SM4_delta=-400;//2001
//long  SM3_SM4_hardelta=5239

long  SM4_start_pos=0l;
long  SM4_end_pos  =0l;
long  SM3_start_pos=0l;
long  SM3_end_pos  =0l;

#define PREPARING_STAGE_CANCEL -1
#define PREPARING_STAGE_1 0
#define PREPARING_STAGE_2 1
#define PREPARING_STAGE_3 2
#define PREPARING_STAGE_4 3

#define HOME_STAGE_CANCEL -1
#define HOME_STAGE_1 4
#define HOME_STAGE_2 5

byte  preparing_state=PREPARING_STAGE_CANCEL;
byte  home_state=HOME_STAGE_CANCEL;

boolean ends=false;
//-----------------------------------------------------------------------
// true if endstoped or reached enstop_pos
//-----------------------------------------------------------------------
boolean moveStepperWhileEndstop(void *s,int endstop_pin,long endstop_pos)
{ AccelStepper *stepper=(AccelStepper *)s; 
  ends=false;
  if(!digitalRead(endstop_pin))
  { stepper->stop();
    ends=true;
    return true;
  } 
  stepper->run();
  if(stepper->currentPosition() == endstop_pos)  
  { stepper->stop();
    return true;
  }
  return false;
}
//-----------------------------------------------------------------------
void moveStepperTo(void *s,long pos)
{ AccelStepper *stepper=(AccelStepper *)s;  
  stepper->setCurrentPosition(stepper->currentPosition());
  stepper->moveTo(pos);
}
void save()
{   EEPROM_write(0, SM4_start_pos);
    EEPROM_write(4, SM4_end_pos);
    EEPROM_write(8, SM3_start_pos);
    EEPROM_write(12, SM3_end_pos);    
}
void load()
{   EEPROM_read(0,SM4_start_pos);
    EEPROM_read(4,SM4_end_pos);
    EEPROM_read(8,SM3_start_pos);
    EEPROM_read(12,SM3_end_pos);
    current_state=FIRST_1;
    SM_4.setCurrentPosition(0); 
    moveStepperTo(&SM_4,7500);
    showMessage("Homing Reel...","don't push");
}
void beep(int length_time)
{ for (int i=0; i<length_time; i++) 
  {  // generate a 1KHz tone for 1/2 second
     digitalWrite(LCD_SPEAKER, HIGH);
     delayMicroseconds(800);
     digitalWrite(LCD_SPEAKER, LOW);
     delayMicroseconds(800);
  }
}
//////////////////////////////////////////////////////////////////////
float  twist_speed=0.01f;
float  twist_step=10.0f;

float  SM4_start_speed  =0.001f;
float  SM1_2_start_speed=1000.0f;
int reel_flag=0;
//////////////////////////////////////////////////////////////////////
float coil_direction=1.0f;
//////////////////////////////////////////////////////////////////////
#define MEM_LEN_TOTAL 300
float           memory_speed[MEM_LEN_TOTAL];
unsigned long   memory_time [MEM_LEN_TOTAL];

int             mem_len=0;
int             play_mem_index=0;
unsigned long   begin_mem_time=0l;
//--------------------------------------------------------------------
void init_memory()
{ twist_speed=0.01f;
  begin_mem_time=millis();
  mem_len=0; 
  memory_speed[mem_len]  =twist_speed;
  memory_time[mem_len]   =0l;
  mem_len++;
}
//--------------------------------------------------------------------
void put_memory(float speed_val)
{  memory_speed[mem_len]=speed_val;
   memory_time[mem_len]=millis()-begin_mem_time;
   mem_len++;
   if(mem_len>=MEM_LEN_TOTAL)
   {mem_len=0;
    beep(300);
   }
}
//--------------------------------------------------------------------
void save_memory()
{   int index=16;  
    EEPROM_write(index,mem_len);
    index+=sizeof(int);
    EEPROM_write(index, memory_speed);
    index+=MEM_LEN_TOTAL*sizeof(float);
    EEPROM_write(index, memory_time);
}
//--------------------------------------------------------------------
void load_memory()
{   int index=16;  
    EEPROM_read(index,mem_len);
    index+=sizeof(int);
    EEPROM_read(index, memory_speed);
    index+=MEM_LEN_TOTAL*sizeof(float);
    EEPROM_read(index, memory_time);
}
//--------------------------------------------------------------------
void start_memory()
{ begin_mem_time=millis();
  play_mem_index=0; 
  twist_speed=memory_speed[play_mem_index];
  play_mem_index++;
}
//---------------------------------------------------------------------
boolean next_memory()
{ if(play_mem_index>=mem_len) 
  {  beep(300);
     return true;
  }
  
  long  now=millis()-begin_mem_time;
  if(now < memory_time[play_mem_index])
  { return false;
  }
  else
  {  play_mem_index++;
  }

    twist_speed=memory_speed[play_mem_index-1];
    if(twist_speed<=-1.0f)   
    { //showBounds(3,"index=",(long)play_mem_index,now);
      beep(300);
      return true;
    }
    SM_3.setSpeed(-twist_speed);
    lcd.setCursor(0,0);
    lcd.print("Speed="); lcd.print(SM_3.speed());lcd.print("         ");      
  return false;
}
///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void setup() 
{ lcd.clear(); lcd.begin(20, 4); lcd.print("Iskanderus AMTwister");
  //--- lcd button init ----------------------------------------------
  pinMode(ROTARY_BUTTON, INPUT);  
  digitalWrite(ROTARY_BUTTON, HIGH);
  //--- endstops init -----------------------------------------------
  pinMode(END_STOP_1, INPUT);
  pinMode(END_STOP_2, INPUT);
  pinMode(END_STOP_3, INPUT);
  //---- steppers --------------------------------------------------
  initStepper(&SM_1,SM_1_ENABLE_PIN);
  initStepper(&SM_2,SM_2_ENABLE_PIN);
  initStepper(&SM_3,SM_3_ENABLE_PIN);
  initStepper(&SM_4,SM_4_ENABLE_PIN);
  //------------------------------------------------------------
  setTwisterStepper(&SM_1,100*16,20000);
  setTwisterStepper(&SM_2,100*16,20000);
  //---- menu --------------------------------------------------
  menuSetup();
  lastTime = millis();
  load();
  pinMode(LCD_SPEAKER, OUTPUT);
}
//////////////////////////////////////////////////////////////////////
long reel_time_start=0l;
int  show_mem_index=0;
//////////////////////////////////////////////////////////////////////
void loop() 
{  
  //---- Rotary button pushed ----------------
  if( (millis()-lastTime) >1000) 
  {   if(!digitalRead(ROTARY_BUTTON))//if button pressed
      {  if(current_state==JUST_MENU)
         {  current_state=menu->rotary_changed(ROTARY_CONFIRM);
            if(current_state==JUST_MENU) 
            {  menu->show();
            }
            else //the other states
            {  process_lcd_state();
               init_stepper_state();
            }
         }
         else // if button pressed process state
         {  process_button_state();
         }
      lastTime = millis();           
      }
    //------------------------------------------   
 
    //lcd.setCursor(0,0); lcd.print(SM_4.currentPosition());   
    //lcd.setCursor(0,3); lcd.print(menu->mname);   
  }
    //---- Rotary rotated ----------------------    
    unsigned char result = rotary.process();      
    if(result && current_state==JUST_MENU)
    {  menu->rotary_changed(result);    
    }
    //----------------------------------------------------------------------
    if(result && (current_state==TWIST_MENU))
    {   if(result == ROTARY_LEFT)
        {  twist_speed-=twist_step;
           if(twist_speed<0.01f) twist_speed=0.01f; 
        } 
        else 
        if (result == ROTARY_RIGHT) 
        {  twist_speed+=twist_step;
           if(twist_speed>800.0f) twist_speed=800.0f; 
        }
        
        SM_3.setSpeed(-twist_speed);       
        lcd.setCursor(0,3);
        lcd.print("Speed="); lcd.print(SM_3.speed());lcd.print("       ");      
        put_memory(abs(SM_3.speed()));
    }
    //-----------------------------------------------------------------------
    if(result && (current_state==SHOW_MEM_MENU))
    {   if(result == ROTARY_LEFT)
        {  show_mem_index--;
           if(show_mem_index<0) show_mem_index=0;
        } 
        else 
        if (result == ROTARY_RIGHT) 
        { show_mem_index++;
          if(show_mem_index>=mem_len) show_mem_index=mem_len-1;
        }
        
        showTwistMemory(1,show_mem_index,mem_len,memory_speed[show_mem_index],memory_time[show_mem_index]);      
    }
    //-----------------------------------------------------------------------
    if(result && (current_state==COIL_DIR_MENU))
    {   if(result == ROTARY_LEFT)
        {  coil_direction=-1.0f;
        } 
        else 
        if (result == ROTARY_RIGHT) 
        { coil_direction=1.0f;
        }
        showCoilDirection((coil_direction>0.0f)?"contracting muscle":"expanding muscle",interrupt_string,(coil_direction>0.0f)?0:1);
        //showMessage((coil_direction>0.0f)?"Contract direction":"Expand direction",interrupt_string);      
    }
    //----- Stepper loop ----------------------- 
    process_stepper_state();
}
///////////////////////////////////////////////////////////////////////
void process_button_state()
{  switch(current_state)
   {case INFO_MENU:          backToMenu(&mainmenu);
    break;
    case HOME_MENU:          SM_3.stop();SM_4.stop();
                             home_state=HOME_STAGE_CANCEL;
                             backToMenu(&operatemenu);
    break;
    case PREPARE_MENU:       SM_3.stop();SM_4.stop();
                             preparing_state=PREPARING_STAGE_CANCEL;
                             backToMenu(&settingsmenu);
    break;
    case STRETCH_MENU:       SM_3.stop();SM_4.stop();
                             backToMenu(&operatemenu);
    break;
    case REEL_HOME_MENU:     SM_4.stop();
                             backToMenu(&reelmenu);
    break;
    case REEL_START_MENU:    SM_1.stop();SM_2.stop();SM_4.stop();
                             SM_4.setSpeed(200); 
                             backToMenu(&reelmenu);
    break;
    case REEL_RIGHT_MENU:    SM_4.stop();
                             backToMenu(&reelmenu);
    break;
    case TWIST_MENU:         SM_1.stop();SM_2.stop();SM_3.stop();
                             SM_3.setSpeed(200); 
                             put_memory(-1.0f);
                             backToMenu(&twistmenu);
                             //showBounds(3,"memlen=",(long)mem_len,0);
    break;
    case COIL_MENU:          SM_1.stop();SM_2.stop();SM_3.stop();
                             SM_3.setSpeed(200); 
                             backToMenu(&twistmenu);
    break;
    case PLAY_MENU:          SM_1.stop();SM_2.stop();SM_3.stop();
                             SM_3.setSpeed(200); 
                             backToMenu(&twistmenu);
    break;
    case SHOW_MEM_MENU:      backToMenu(&settingsmenu);
    break;
    case COIL_DIR_MENU:      backToMenu(&settingsmenu);
    break;
    
   }
}
void process_lcd_state()
{  switch(current_state)
   {case INFO_MENU:        lcd.clear();
                           //lcd.setCursor(0,1);lcd.print("Info");
                           showBounds(0,"Reel pos=",SM_4.currentPosition(),0);
                           showBounds(1,"Stretch pos=",SM_3.currentPosition(),0);
                           showBounds(2,"Reel ",SM4_start_pos,SM4_end_pos);
                           showBounds(3,"Stretch ",SM3_start_pos,SM3_end_pos);
    break;
    case HOME_MENU:        showMessage("Homing heads...",interrupt_string);
    break;
    case SAVE_MENU:        showMessage("Saving to eeprom...",interrupt_string);
                           //- save eeprom
                           save();
                           beep(100);delay(500);beep(100);
                           backToMenu(&settingsmenu);
    break;    
    case SAVE_TWIST:       showMessage("Saving to eeprom...",interrupt_string);
                           //- save twist to eeprom
                           save_memory();
                           beep(100);delay(500);beep(100);
                           backToMenu(&settingsmenu);
    break;    
    case PREPARE_MENU:     showMessage("Preparing stage 1",interrupt_string);
    break;    
    case STRETCH_MENU:     showMessage("Stretching right...",interrupt_string);
    break;    
    case REEL_HOME_MENU:   showMessage("Homing Reel ...",interrupt_string);
    break;    
    case REEL_START_MENU:   showMessage("Reel the Reel...",interrupt_string);
    break;    
    case REEL_RIGHT_MENU:  showMessage("Moving Reel right...",interrupt_string);
    break;    
    case TWIST_MENU:       showMessage("Twisting...","Rotary changes speed");
    break;
    case COIL_MENU:       showMessage("Coiling...",interrupt_string);
    break;
    case PLAY_MENU:       showMessage("Playing twist...",interrupt_string);
    break;
    case SHOW_MEM_MENU:   showMessage("Show twist memory",interrupt_string);
    break;
    case COIL_DIR_MENU:   showMessage("Set coil direction",interrupt_string);
    break;
   }
}


void init_stepper_state()
{  switch(current_state)
   {case HOME_MENU:      moveStepperTo(&SM_4,SM4_start_pos);
                         home_state=HOME_STAGE_1;
    break; 
    case PREPARE_MENU:   preparing_state=PREPARING_STAGE_1;
                         SM_4.setCurrentPosition(0);   
                         SM_4.moveTo(7500); 
                      
    break;     
    case STRETCH_MENU:       moveStepperTo(&SM_3,SM3_end_pos);
    break; 
    case REEL_HOME_MENU:     moveStepperTo(&SM_4,SM4_start_pos);
    break;    
    case REEL_START_MENU: {   long target=SM3_end_pos-SM_3.currentPosition();//SM3_SM4_hardelta-SM_3.currentPosition();
                              if(target<SM4_end_pos)  target=SM4_end_pos;
                              SM_4.setSpeed(SM4_start_speed);
                              moveStepperTo(&SM_4,target);
                              
                              SM_1.setSpeed(SM1_2_start_speed);
                              SM_2.setSpeed(SM1_2_start_speed);
                              
                              moveStepperTo(&SM_1,10000);
                              moveStepperTo(&SM_2,-10000);
                              
                              //showBounds(0,"Reel tar=",SM_4.targetPosition(),0);
                              //showBounds(1,"Stre pos=",SM_3.currentPosition(),0);
                              reel_time_start=millis();
                          }   
                              
    break;    
    case REEL_RIGHT_MENU:    {long target=SM3_end_pos-SM_3.currentPosition();//SM3_SM4_hardelta-SM_3.currentPosition();
                              if(target<SM4_end_pos)  target=SM4_end_pos;
                              moveStepperTo(&SM_4,target);
                             }
    break;    
    case TWIST_MENU:{  init_memory();
    
                       SM_3.setSpeed(twist_speed);
                       moveStepperTo(&SM_3,SM3_start_pos);

                       SM_2.setSpeed(1000); 
                       SM_2.setCurrentPosition(200*75000);
                       moveStepperTo(&SM_2,0);
                       SM_1.setSpeed(1000); 
                       SM_1.setCurrentPosition(200*75000);
                       moveStepperTo(&SM_1,0);
                    }
    break;    
    case COIL_MENU:{  twist_speed=0.0f;
                      SM_3.setSpeed(twist_speed);
                      //moveStepperTo(&SM_3,SM3_start_pos);

                      SM_2.setCurrentPosition(coil_direction*200*75000);
                      moveStepperTo(&SM_2,0);
                    }
    break;    
    case PLAY_MENU:{   load_memory();
                       if(mem_len==0)
                       {showMessage("Play mem is empty",interrupt_string);
                        beep(300);
                        delay(2000);
                        backToMenu(&twistmenu);
                        return;
                       }
                       start_memory();
    
                       SM_3.setSpeed(twist_speed);
                       moveStepperTo(&SM_3,SM3_start_pos);

                       SM_2.setSpeed(1000); 
                       SM_2.setCurrentPosition(200*75000);
                       moveStepperTo(&SM_2,0);
                       SM_1.setSpeed(1000); 
                       SM_1.setCurrentPosition(200*75000);
                       moveStepperTo(&SM_1,0);
                    }
    break;    
    case SHOW_MEM_MENU:{   load_memory();
                           if(mem_len==0)
                           {showMessage("Show mem is empty",interrupt_string);
                            beep(300);
                            delay(2000);
                            backToMenu(&settingsmenu);
                            return;
                           }
                           show_mem_index=0;
                       }
    break;    
    case COIL_DIR_MENU:{    showCoilDirection((coil_direction>0.0f)?"contracting muscle":"expanding muscle",interrupt_string,(coil_direction>0.0f)?0:1);
                           
                       }
    break;    
    default:  
    break;
   }
}

void process_stepper_state()
{  switch(current_state)
   {case HOME_MENU:  {  
                        switch(home_state)
                        {   case HOME_STAGE_1:
                               if(moveStepperWhileEndstop(&SM_4,END_STOP_3,SM4_start_pos))
                               {  showMessage("Homing stage 2",interrupt_string);
                                  SM_4.stop();
                                  moveStepperTo(&SM_3,SM3_start_pos);
                                  home_state=HOME_STAGE_2;
                               }
                              break;
                            case HOME_STAGE_2:
                               if(moveStepperWhileEndstop(&SM_3,END_STOP_1,SM3_start_pos))
                               { home_state=HOME_STAGE_CANCEL;
                                 SM_3.stop();
                                 backToMenu(&operatemenu);
                               }
                              break;   
                         };
                       }
    break;               
    case FIRST_1:    if(moveStepperWhileEndstop(&SM_4,END_STOP_3,7500))
                     { SM_4.setCurrentPosition(SM4_start_pos); 
                       SM_3.setCurrentPosition(7500);
                       moveStepperTo(&SM_3,0);
                       showMessage("Homing Stretch...","don't push");
                       current_state=FIRST_2;
                      }
    break;               
    case FIRST_2:    if(moveStepperWhileEndstop(&SM_3,END_STOP_1,0))
                     { SM_3.setCurrentPosition(SM3_start_pos); 
                       beep(100);                      
                       backToMenu(&mainmenu); 
                      }
    break;               
    case PREPARE_MENU:  {  switch(preparing_state)
                           { case PREPARING_STAGE_1:
                               if(moveStepperWhileEndstop(&SM_4,END_STOP_3,7500))
                               {  SM_3.setCurrentPosition(7500);
                                  SM_3.moveTo(0); 
                                  preparing_state=PREPARING_STAGE_2;
                                  showMessage("Preparing stage 2",interrupt_string);
                               }
                            break;
                            case PREPARING_STAGE_2:
                               if(moveStepperWhileEndstop(&SM_3,END_STOP_1,0))
                               { SM_3.setCurrentPosition(0); 
                                 SM3_start_pos=0;
                                 SM_3.moveTo(7500); 
                                 preparing_state=PREPARING_STAGE_3;
                                 showMessage("Preparing stage 3",interrupt_string);
                               }
                            break;     
                            case PREPARING_STAGE_3:
                               if(moveStepperWhileEndstop(&SM_3,END_STOP_2,7500))
                               { SM3_end_pos=SM_3.currentPosition();
                                 SM4_start_pos=SM3_end_pos-SM3_SM4_delta;
                                 SM_4.setCurrentPosition(SM4_start_pos); 
                                 SM_4.moveTo(0);
                                 preparing_state=PREPARING_STAGE_4;
                                 showMessage("Preparing stage 4",interrupt_string);
                               }
                            break;     
                            case PREPARING_STAGE_4:
                               if(moveStepperWhileEndstop(&SM_4,END_STOP_1,0))
                               { SM4_end_pos=0;
                                 SM_4.stop();
                                 preparing_state=PREPARING_STAGE_CANCEL;
                                 backToMenu(&settingsmenu);
                               }
                            break;   
                             };
                            
                           }
    break;                   
    case STRETCH_MENU:  if(moveStepperWhileEndstop(&SM_3,END_STOP_2,SM3_end_pos))
                        { SM_3.stop(); 
                          backToMenu(&operatemenu);
                        }
    break;
    case REEL_HOME_MENU:  if(moveStepperWhileEndstop(&SM_4,END_STOP_3,SM4_start_pos))
                          { SM_4.stop();
                            backToMenu(&reelmenu);
                          }
    break;               
    case REEL_START_MENU: 
                          if( SM_4.distanceToGo()!=0)  
                          { if(reel_flag==2500) 
                              { SM_4.runSpeed();
                                reel_flag=0;
                                long sec=(millis()-reel_time_start)/1000;
                                long minutes=sec/60;
                                showBounds(3,"Time=",minutes,sec-minutes*60);
                              }
                            reel_flag++; 
                          }
                          else
                          {  SM_1.stop();SM_2.stop();SM_4.stop();   
                             backToMenu(&reelmenu);
                             beep(200);
                             break;
                          }
                          if( SM_1.distanceToGo()==0)  moveStepperTo(&SM_1,SM_1.currentPosition()+10000);
                          SM_1.runSpeed();
                          if( SM_2.distanceToGo()==0)  moveStepperTo(&SM_2,SM_2.currentPosition()-10000);
                          SM_2.runSpeed();
                          
    break;               
    case REEL_RIGHT_MENU:  if( SM_4.distanceToGo()!=0)  
                           { SM_4.run();
                           }
                           else
                           { SM_4.stop(); 
                             backToMenu(&reelmenu);
                           }
    break;
    case TWIST_MENU:      {  if( SM_3.distanceToGo()!=0)  
                             {  SM_3.runSpeed();
                             } 
                             else
                             {  SM_1.stop();SM_2.stop();SM_3.stop();
                                put_memory(-1.0f);
                                backToMenu(&operatemenu);
                                //showBounds(3,"mem_len=",(long)mem_len,0);
                                return;
                             }
      
                             if( SM_2.distanceToGo()!=0)  
                             {  SM_2.runSpeed();
                             } 
                             if( SM_1.distanceToGo()!=0)  
                             {  SM_1.runSpeed();
                             } 
                          }
    break;    
    case COIL_MENU:      {  if( SM_3.distanceToGo()!=0)  
                             {  SM_3.runSpeed();
                             } 
                             else
                             {  SM_1.stop();SM_2.stop();SM_3.stop();
                                backToMenu(&operatemenu);
                                return;
                             }
      
                             if( SM_2.distanceToGo()!=0)  
                             {  SM_2.runSpeed();
                             } 
                          }
    break;    
    case PLAY_MENU:      {   if(next_memory())//if play is over
                             {  SM_1.stop();SM_2.stop();SM_3.stop();
                                backToMenu(&operatemenu);
                                return;
                             }
                             
                             if( SM_3.distanceToGo()!=0)  
                             {  SM_3.runSpeed();
                             } 
                             else
                             {  SM_1.stop();SM_2.stop();SM_3.stop();
                                backToMenu(&operatemenu);
                                return;
                             }
      
                             if( SM_2.distanceToGo()!=0)  
                             {  SM_2.runSpeed();
                             } 
                             if( SM_1.distanceToGo()!=0)  
                             {  SM_1.runSpeed();
                             } 
                          }
    break;    
    default:
    break;
   }
}

//--------------------------------------------------------------------

