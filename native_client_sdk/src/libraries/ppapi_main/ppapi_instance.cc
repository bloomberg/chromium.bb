// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "nacl_io/kernel_wrap.h"
#include "nacl_io/nacl_io.h"

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/touch_point.h"
#include "ppapi/cpp/var.h"

#include "ppapi_main/ppapi_event.h"
#include "ppapi_main/ppapi_instance.h"
#include "ppapi_main/ppapi_main.h"


PPAPIInstance* PPAPI_GetInstance() {
  return static_cast<PPAPIInstance*>(PPAPI_GetInstanceObject());
}

struct StartInfo {
  PPAPIInstance* inst_;
  uint32_t argc_;
  const char** argv_;
};

void* PPAPIInstance::StartMain(void *info) {
  StartInfo* si = static_cast<StartInfo*>(info);
  si->inst_->main_loop_.AttachToCurrentThread();

  if (NULL != info) {
    ppapi_main(si->argc_, si->argv_);

    for (uint32_t i = 0; i < si->argc_; i++) {
      delete[] si->argv_[i];
    }
    delete[] si->argv_;
    delete si;
  }
  else {
    const char *argv[] = { "NEXE", NULL };
    ppapi_main(1, argv);
  }

  return NULL;
}

PPAPIInstance::PPAPIInstance(PP_Instance instance, const char *args[])
    : pp::Instance(instance),
      main_loop_(this),
      has_focus_(false) {
  while (*args) {
    std::string key = *args++;
    std::string val = *args++;
    properties_[key] = val;
  }

  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                     PP_INPUTEVENT_CLASS_KEYBOARD |
                     PP_INPUTEVENT_CLASS_WHEEL |
                     PP_INPUTEVENT_CLASS_TOUCH);
}

PPAPIInstance::~PPAPIInstance() {
}

bool PPAPIInstance::Init(uint32_t arg,
                         const char* argn[],
                         const char* argv[]) {
  StartInfo* si = new StartInfo;

  si->inst_ = this;
  si->argc_ = 1;
  si->argv_ = new const char *[arg*2+1];
  si->argv_[0] = NULL;

  for (uint32_t i=0; i < arg; i++) {
    // If we start with PM prefix set the instance argument map
    if (0 == strncmp(argn[i], "pm_", 3)) {
      std::string key = argn[i];
      std::string val = argv[i];
      properties_[key] = val;
      continue;
    }

    // If this is the 'src' tag, then get the NMF name.
    if (!strcmp("src", argn[i])) {
      char *name = new char[strlen(argv[i]) + 1];
      strcpy(name, argv[i]);
      si->argv_[0] = name;
    }
    else {
      // Otherwise turn it into arguments
      char *key = new char[strlen(argn[i]) + 3];
      key[0] = '-';
      key[1] = '-';
      strcpy(&key[2], argn[i]);

      si->argv_[si->argc_++] = key;
      if (argv[i] && argv[i][0]) {
        char *val = new char[strlen(argv[i]) + 1];
        strcpy(val, argv[i]);
        si->argv_[si->argc_++] = val;
      }
    }
  }

  // If src was not found, set the first arg to something
  if (NULL == si->argv_[0]) {
    char *name = new char[5];
    strcpy(name, "NMF?");
    si->argv_[0] = name;
  }

  if (ProcessProperties()) {
    pthread_t main_thread;
    int ret = pthread_create(&main_thread, NULL, StartMain,
                             static_cast<void*>(si));
    return ret == 0;
  }

  return false;
}

const char* PPAPIInstance::GetProperty(const char* key, const char* def) {
  PropertyMap_t::iterator it = properties_.find(key);
  if (it != properties_.end()) {
    return it->second.c_str();
  }
  return def;
}

bool PPAPIInstance::ProcessProperties() {
  const char* stdin_path = GetProperty("pm_stdin", "/dev/stdin");
  const char* stdout_path = GetProperty("pm_stdout", "/dev/stdout");
  const char* stderr_path = GetProperty("pm_stderr", "/dev/console3");
  const char* queue_size = GetProperty("pm_queue_size", "1024");

  // Force a minimum size of 4
  uint32_t queue_size_int = atoi(queue_size);
  if (queue_size_int < 4) queue_size_int = 4;
  event_queue_.SetSize(queue_size_int);

  nacl_io_init_ppapi(PPAPI_GetInstanceId(), PPAPI_GetInterface);

  int fd0 = open(stdin_path, O_RDONLY);
  dup2(fd0, 0);

  int fd1 = open(stdout_path, O_WRONLY);
  dup2(fd1, 1);

  int fd2 = open(stderr_path, O_WRONLY);
  dup2(fd2, 2);

  setvbuf(stderr, NULL, _IOLBF, 0);
  setvbuf(stdout, NULL, _IOLBF, 0);
  return true;
}

void PPAPIInstance::HandleMessage(const pp::Var& message) {}


bool PPAPIInstance::HandleInputEvent(const pp::InputEvent& event) {
  PPAPIEvent* event_ptr;

  // Remove a stale message if one is available
  event_ptr = static_cast<PPAPIEvent*>(event_queue_.RemoveStaleMessage());
  delete event_ptr;

  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE: {
      pp::MouseInputEvent mouse_event(event);
      PPAPIMouseEvent* mouse_ptr = new PPAPIMouseEvent;
      mouse_ptr->button = mouse_event.GetButton();
      mouse_ptr->location = mouse_event.GetPosition().pp_point();
      mouse_ptr->delta = mouse_event.GetMovement().pp_point();
      event_ptr = &mouse_ptr->event;
      break;
    }

    case PP_INPUTEVENT_TYPE_WHEEL: {
      pp::WheelInputEvent wheel_event(event);
      PPAPIWheelEvent* wheel_ptr = new PPAPIWheelEvent;
      wheel_ptr->by_page =
          static_cast<uint32_t>(wheel_event.GetScrollByPage());
      wheel_ptr->delta = wheel_event.GetDelta().pp_float_point();
      event_ptr = &wheel_ptr->event;
      break;
    }

    case PP_INPUTEVENT_TYPE_CHAR: {
       pp::KeyboardInputEvent key_event(event);
       PPAPICharEvent* char_ptr = new PPAPICharEvent;
       strncpy(char_ptr->text,
               key_event.GetCharacterText().DebugString().c_str(),
               sizeof(char_ptr->text));
       event_ptr = &char_ptr->event;
      break;
    }

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
      pp::KeyboardInputEvent key_event(event);
      PPAPIKeyEvent* key_ptr = new PPAPIKeyEvent;
      key_ptr->key_code = key_event.GetKeyCode();
      event_ptr = &key_ptr->event;
      break;
    }

    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL: {
      pp::TouchInputEvent touch_event(event);
      PPAPITouchEvent* touch_ptr = new PPAPITouchEvent;
      touch_ptr->point_count =
          touch_event.GetTouchCount(PP_TOUCHLIST_TYPE_TOUCHES);
      for (uint32_t cnt = 0; cnt < touch_ptr->point_count; cnt++) {
          pp::TouchPoint *pnt = (pp::TouchPoint*) &touch_ptr->points[cnt];
          *pnt = touch_event.GetTouchByIndex(PP_TOUCHLIST_TYPE_TOUCHES ,cnt);
      }
      event_ptr = &touch_ptr->event;
      break;
    }

    default:
      fprintf(stderr, "Unhandled event type %d\n", event.GetType());
      return false;
  }

  event_ptr->event_type = event.GetType();
  event_ptr->time_ticks = event.GetTimeStamp();
  event_ptr->modifiers = event.GetModifiers();
  if (!event_queue_.AddNewMessage(event_ptr)) {
    printf("Warning:  Event Queue is full, dropping message.\n");
  }
  return true;
}

PPAPIEvent* PPAPIInstance::AcquireInputEvent() {
  return static_cast<PPAPIEvent*>(event_queue_.AcquireTopMessage());
}

void PPAPIInstance::ReleaseInputEvent(PPAPIEvent* event) {
  event_queue_.ReleaseTopMessage(event);
}

void PPAPIInstance::DidChangeView(const pp::View&) {
}

void PPAPIInstance::DidChangeFocus(bool focus) {
  has_focus_ = focus;
}
