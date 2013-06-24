// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
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

#include "ppapi/c/ppb_var.h"

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/touch_point.h"
#include "ppapi/cpp/var.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_main.h"

#if defined(WIN32)
#define open _open
#define dup2 _dup2
#endif

static PSInstance* s_InstanceObject = NULL;

PSInstance* PSInstance::GetInstance() {
  return s_InstanceObject;
}

struct StartInfo {
  PSInstance* inst_;
  uint32_t argc_;
  char** argv_;
};


// The starting point for 'main'.  We create this thread to hide the real
// main pepper thread which must never be blocked.
void* PSInstance::MainThreadThunk(void *info) {
  s_InstanceObject->Log("Got MainThreadThunk.\n");
  StartInfo* si = static_cast<StartInfo*>(info);
  si->inst_->main_loop_ = new pp::MessageLoop(si->inst_);
  si->inst_->main_loop_->AttachToCurrentThread();

  int ret = si->inst_->MainThread(si->argc_, si->argv_);
  for (uint32_t i = 0; i < si->argc_; i++) {
    delete[] si->argv_[i];
  }
  delete[] si->argv_;
  delete si;

  return NULL;
}

// The default implementation supports running a 'C' main.
int PSInstance::MainThread(int argc, char *argv[]) {
  if (main_cb_) {
    Log("Starting MAIN.\n");
    int ret = main_cb_(argc, argv);
    Log("Main thread returned with %d.\n", ret);
    return ret;
  }

  Error("No main defined.\n");
  return 0;
}

PSInstance::PSInstance(PP_Instance instance, const char *argv[])
    : pp::Instance(instance),
      pp::MouseLock(this),
      pp::Graphics3DClient(this),
      main_loop_(NULL),
      verbosity_(PSV_LOG),
      events_enabled_(PSE_NONE) {
  // Set the single Instance object
  s_InstanceObject = this;

#ifdef DEBUG
  SetVerbosity(PSV_LOG);
#endif

  // Place PPAPI_MAIN_USE arguments into properties map
  while (*argv) {
    std::string key = *argv++;
    std::string val = *argv++;
    properties_[key] = val;
  }

  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                     PP_INPUTEVENT_CLASS_KEYBOARD |
                     PP_INPUTEVENT_CLASS_WHEEL |
                     PP_INPUTEVENT_CLASS_TOUCH);
}

PSInstance::~PSInstance() {}

void PSInstance::SetMain(PSMainFunc_t main) {
  main_cb_ = main;
}

bool PSInstance::Init(uint32_t arg,
                      const char* argn[],
                      const char* argv[]) {
  StartInfo* si = new StartInfo;

  si->inst_ = this;
  si->argc_ = 1;
  si->argv_ = new char *[arg*2+1];
  si->argv_[0] = NULL;

  // Process arguments passed into Module INIT from JavaScript
  for (uint32_t i=0; i < arg; i++) {
    if (argv[i]) {
      Log("ARG %s=%s\n", argn[i], argv[i]);
    } else {
      Log("ARG %s\n", argn[i]);
    }

    // If we start with PM prefix set the instance argument map
    if (0 == strncmp(argn[i], "ps_", 3)) {
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

  PSInterfaceInit();

  if (ProcessProperties()) {
    pthread_t main_thread;
    int ret = pthread_create(&main_thread, NULL, MainThreadThunk, si);
    Log("Created thread: %d.\n", ret);
    return ret == 0;
  }

  Log("Skipping create thread.\n");
  return false;
}

const char* PSInstance::GetProperty(const char* key, const char* def) {
  PropertyMap_t::iterator it = properties_.find(key);
  if (it != properties_.end()) {
    return it->second.c_str();
  }
  return def;
}

// Processes the properties passed fixed at compile time via the
// initialization macro, or via dynamically set embed attributes
// through instance DidCreate.
bool PSInstance::ProcessProperties() {
  // Get the default values
  const char* stdin_path = GetProperty("ps_stdin", "/dev/stdin");
  const char* stdout_path = GetProperty("ps_stdout", "/dev/stdout");
  const char* stderr_path = GetProperty("ps_stderr", "/dev/console3");
  const char* verbosity = GetProperty("ps_verbosity", NULL);

  // Reset verbosity if passed in
  if (verbosity) SetVerbosity(static_cast<Verbosity>(atoi(verbosity)));

  // Enable NaCl IO to map STDIN, STDOUT, and STDERR
  nacl_io_init_ppapi(PSGetInstanceId(), PSGetInterface);
  int fd0 = open(stdin_path, O_RDONLY);
  dup2(fd0, 0);

  int fd1 = open(stdout_path, O_WRONLY);
  dup2(fd1, 1);

  int fd2 = open(stderr_path, O_WRONLY);
  dup2(fd2, 2);

  // Set line buffering on stdout and stderr
#if !defined(WIN32)
  setvbuf(stderr, NULL, _IOLBF, 0);
  setvbuf(stdout, NULL, _IOLBF, 0);
#endif
  return true;
}

void PSInstance::SetVerbosity(Verbosity verbosity) {
  verbosity_ = verbosity;
}

void PSInstance::Log(const char *fmt, ...) {
  if (verbosity_ >= PSV_LOG) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
  }
}

void PSInstance::Warn(const char *fmt, ...) {
  if (verbosity_ >= PSV_WARN) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
  }
}

void PSInstance::Error(const char *fmt, ...) {
  if (verbosity_ >= PSV_ERROR) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
  }
}

void PSInstance::SetEnabledEvents(uint32_t mask) {
  events_enabled_ = mask;
}

void PSInstance::PostEvent(PSEventType type) {
  assert(PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST == type ||
         PSE_MOUSELOCK_MOUSELOCKLOST == type);

  if (events_enabled_ & type) {
    PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
    memset(env, 0, sizeof(*env));
    env->type = type;
    event_queue_.Enqueue(env);
  }
}

void PSInstance::PostEvent(PSEventType type, PP_Bool bool_value) {
  assert(PSE_INSTANCE_DIDCHANGEFOCUS == type);

  if (events_enabled_ & type) {
    PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
    memset(env, 0, sizeof(*env));
    env->type = type;
    env->as_bool = bool_value;
    event_queue_.Enqueue(env);
  }
}

void PSInstance::PostEvent(PSEventType type, PP_Resource resource) {
  assert(PSE_INSTANCE_HANDLEINPUT == type ||
         PSE_INSTANCE_DIDCHANGEVIEW == type);

  if (events_enabled_ & type) {
    if (resource) {
      PSInterfaceCore()->AddRefResource(resource);
    }
    PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
    memset(env, 0, sizeof(*env));
    env->type = type;
    env->as_resource = resource;
    event_queue_.Enqueue(env);
  }
}

void PSInstance::PostEvent(PSEventType type, const PP_Var& var) {
  assert(PSE_INSTANCE_HANDLEMESSAGE == type);

  if (events_enabled_ & type) {
    PSInterfaceVar()->AddRef(var);
    PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
    memset(env, 0, sizeof(*env));
    env->type = type;
    env->as_var = var;
    event_queue_.Enqueue(env);
  }
}

PSEvent* PSInstance::TryAcquireEvent() {
  return event_queue_.Dequeue(false);
}

PSEvent* PSInstance::WaitAcquireEvent() {
  return event_queue_.Dequeue(true);
}

void PSInstance::ReleaseEvent(PSEvent* event) {
  if (event) {
    switch(event->type) {
      case PSE_INSTANCE_HANDLEMESSAGE:
        PSInterfaceVar()->Release(event->as_var);
        break;
      case PSE_INSTANCE_HANDLEINPUT:
      case PSE_INSTANCE_DIDCHANGEVIEW:
        if (event->as_resource) {
          PSInterfaceCore()->ReleaseResource(event->as_resource);
        }
        break;
      default:
        break;
    }
    free(event);
  }
}

void PSInstance::HandleMessage(const pp::Var& message) {
  Log("Got Message\n");
  PostEvent(PSE_INSTANCE_HANDLEMESSAGE, message.pp_var());
}

bool PSInstance::HandleInputEvent(const pp::InputEvent& event) {
  PostEvent(PSE_INSTANCE_HANDLEINPUT, event.pp_resource());
  return true;
}

void PSInstance::DidChangeView(const pp::View& view) {
  pp::Size new_size = view.GetRect().size();
  Log("Got View change: %d,%d\n", new_size.width(), new_size.height());
  PostEvent(PSE_INSTANCE_DIDCHANGEVIEW, view.pp_resource());
}

void PSInstance::DidChangeFocus(bool focus) {
  Log("Got Focus change: %s\n", focus ? "FOCUS ON" : "FOCUS OFF");
  PostEvent(PSE_INSTANCE_DIDCHANGEFOCUS, focus ? PP_TRUE : PP_FALSE);
}

void PSInstance::Graphics3DContextLost() {
  Log("Graphics3DContextLost\n");
  PostEvent(PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST);
}

void PSInstance::MouseLockLost() {
  Log("MouseLockLost\n");
  PostEvent(PSE_MOUSELOCK_MOUSELOCKLOST);
}
