// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "nacl_mounts/kernel_wrap.h"
#include "nacl_mounts/nacl_mounts.h"

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"

#include "ppapi_main/ppapi_instance.h"
#include "ppapi_main/ppapi_main.h"


struct StartInfo {
  uint32_t argc_;
  const char** argv_;
};

static void* StartMain(void *info) {
  StartInfo* si = static_cast<StartInfo*>(info);

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
      has_focus_(false) {

  while (*args) {
    std::string key = *args++;
    std::string val = *args++;
    properties_[key] = val;
  }

  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
}

PPAPIInstance::~PPAPIInstance() {
}

bool PPAPIInstance::Init(uint32_t arg,
                         const char* argn[],
                         const char* argv[]) {
  StartInfo* si = new StartInfo;

  si->argc_ = 1;
  si->argv_ = new const char *[arg*2+1];
  si->argv_[0] = NULL;

  for (uint32_t i=0; i < arg; i++) {
    // If we start with PM prefix set the instance argument map
    if (0 == strncmp(argn[i], "PM_", 3)) {
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
  PropteryMap_t::iterator it = properties_.find(key);
  if (it != properties_.end()) {
    return it->second.c_str();
  }
  return def;
}

bool PPAPIInstance::ProcessProperties() {
  const char* stdin_path = GetProperty("PM_STDIO", "/dev/null");
  const char* stdout_path = GetProperty("PM_STDOUT", "/dev/tty");
  const char* stderr_path = GetProperty("PM_STDERR", "/dev/console3");

  nacl_mounts_init_ppapi(PPAPI_GetInstanceId(), PPAPI_GetInterface);
  int f1 = open(stdin_path, O_RDONLY);
  int f2 = open(stdout_path, O_WRONLY);
  int f3 = open(stderr_path, O_WRONLY);

  return true;
}

void PPAPIInstance::HandleMessage(const pp::Var& message) {
}


bool PPAPIInstance::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      break;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      // If we do not yet have focus, return true. In return Chrome will give
      // focus to the NaCl embed.
      return !has_focus_;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_WHEEL:
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
    case PP_INPUTEVENT_TYPE_CHAR:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
    default:
      return false;
  }
  return false;
}

void PPAPIInstance::DidChangeView(const pp::View&) {
}

void PPAPIInstance::DidChangeFocus(bool focus) {
  has_focus_ = focus;
}
