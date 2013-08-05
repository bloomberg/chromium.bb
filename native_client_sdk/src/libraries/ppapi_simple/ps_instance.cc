// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "nacl_io/ioctl.h"
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
  s_InstanceObject->Trace("Got MainThreadThunk.\n");
  StartInfo* si = static_cast<StartInfo*>(info);
  si->inst_->main_loop_ = new pp::MessageLoop(si->inst_);
  si->inst_->main_loop_->AttachToCurrentThread();

  int ret = si->inst_->MainThread(si->argc_, si->argv_);
  for (uint32_t i = 0; i < si->argc_; i++) {
    delete[] si->argv_[i];
  }
  delete[] si->argv_;
  delete si;

  // Exit the entire process once the 'main' thread returns.
  // The error code will be available to javascript via
  // the exitcode paramater of the crash event.
  exit(ret);
  return NULL;
}

// The default implementation supports running a 'C' main.
int PSInstance::MainThread(int argc, char *argv[]) {
  if (!main_cb_) {
    Error("No main defined.\n");
    return 0;
  }

  Trace("Starting MAIN.\n");
  int ret = main_cb_(argc, argv);
  Log("Main thread returned with %d.\n", ret);
  return ret;
}

PSInstance::PSInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::MouseLock(this),
      pp::Graphics3DClient(this),
      main_loop_(NULL),
      events_enabled_(PSE_NONE),
      verbosity_(PSV_WARN),
      fd_tty_(-1) {
  // Set the single Instance object
  s_InstanceObject = this;

#ifdef NACL_SDK_DEBUG
  SetVerbosity(PSV_LOG);
#endif

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
  si->argc_ = 0;
  si->argv_ = new char *[arg+1];
  si->argv_[0] = NULL;

  // Process embed attributes into the environment.
  // Converted the attribute names to uppercase as environment variables are
  // case sensitive but are almost universally uppercase in practice.
  for (uint32_t i = 0; i < arg; i++) {
    std::string key = argn[i];
    std::transform(key.begin(), key.end(), key.begin(), toupper);
    setenv(key.c_str(), argv[i], 1);
  }

  // Set a default value for SRC.
  setenv("SRC", "NMF?", 0);
  // Use the src tag name if ARG0 is not explicitly specified.
  setenv("ARG0", getenv("SRC"), 0);

  // Walk ARG0..ARGn populating argv until an argument is missing.
  for (;;) {
    std::ostringstream arg_stream;
    arg_stream << "ARG" << si->argc_;
    std::string arg_name = arg_stream.str();
    const char* next_arg = getenv(arg_name.c_str());
    if (NULL == next_arg)
      break;

    char* value = new char[strlen(next_arg) + 1];
    strcpy(value, next_arg);
    si->argv_[si->argc_++] = value;
  }

  PSInterfaceInit();
  bool props_processed = ProcessProperties();

  // Log arg values only once ProcessProperties has been
  // called so that the ps_verbosity attribute will be in
  // effect.
  for (uint32_t i = 0; i < arg; i++) {
    if (argv[i]) {
      Trace("attribs[%d] '%s=%s'\n", i, argn[i], argv[i]);
    } else {
      Trace("attribs[%d] '%s'\n", i, argn[i]);
    }
  }

  for (uint32_t i = 0; i < si->argc_; i++) {
    Trace("argv[%d] '%s'\n", i, si->argv_[i]);
  }

  if (!props_processed) {
    Warn("Skipping create thread.\n");
    return false;
  }

  pthread_t main_thread;
  int ret = pthread_create(&main_thread, NULL, MainThreadThunk, si);
  Trace("Created thread: %d.\n", ret);
  return ret == 0;
}

// Processes the properties set at compile time via the
// initialization macro, or via dynamically set embed attributes
// through instance DidCreate.
bool PSInstance::ProcessProperties() {
  // Set default values
  setenv("PS_STDIN", "/dev/stdin", 0);
  setenv("PS_STDOUT", "/dev/stdout", 0);
  setenv("PS_STDERR", "/dev/console3", 0);

  // Reset verbosity if passed in
  const char* verbosity = getenv("PS_VERBOSITY");
  if (verbosity) SetVerbosity(static_cast<Verbosity>(atoi(verbosity)));

  // Enable NaCl IO to map STDIN, STDOUT, and STDERR
  nacl_io_init_ppapi(PSGetInstanceId(), PSGetInterface);
  int fd0 = open(getenv("PS_STDIN"), O_RDONLY);
  dup2(fd0, 0);

  int fd1 = open(getenv("PS_STDOUT"), O_WRONLY);
  dup2(fd1, 1);

  int fd2 = open(getenv("PS_STDERR"), O_WRONLY);
  dup2(fd2, 2);

  const char* tty_prefix = getenv("PS_TTY_PREFIX");
  if (tty_prefix) {
    fd_tty_ = open("/dev/tty", O_WRONLY);
    if (fd_tty_ >= 0) {
      ioctl(fd_tty_, TIOCNACLPREFIX, const_cast<char*>(tty_prefix));
    } else {
      Error("Failed to open /dev/tty.\n");
    }
  }

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

void PSInstance::VALog(Verbosity verbosity, const char *fmt, va_list args) {
  if (verbosity <= verbosity_) {
    fprintf(stderr, "ps: ");
    vfprintf(stderr, fmt, args);
  }
}

void PSInstance::Trace(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_TRACE, fmt, ap);
  va_end(ap);
}

void PSInstance::Log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_LOG, fmt, ap);
  va_end(ap);
}

void PSInstance::Warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_WARN, fmt, ap);
  va_end(ap);
}

void PSInstance::Error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_ERROR, fmt, ap);
  va_end(ap);
}

void PSInstance::SetEnabledEvents(uint32_t mask) {
  events_enabled_ = mask;
  if (mask == 0) {
    static bool warn_once = true;
    if (warn_once) {
      Warn("PSInstance::SetEnabledEvents(mask) where mask == 0 will block\n");
      Warn("all events. This can come from PSEventSetFilter(PSE_NONE);\n");
      warn_once = false;
    }
  }
}

void PSInstance::PostEvent(PSEventType type) {
  assert(PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST == type ||
         PSE_MOUSELOCK_MOUSELOCKLOST == type);

  PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
  memset(env, 0, sizeof(*env));
  env->type = type;
  event_queue_.Enqueue(env);
}

void PSInstance::PostEvent(PSEventType type, PP_Bool bool_value) {
  assert(PSE_INSTANCE_DIDCHANGEFOCUS == type);

  PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
  memset(env, 0, sizeof(*env));
  env->type = type;
  env->as_bool = bool_value;
  event_queue_.Enqueue(env);
}

void PSInstance::PostEvent(PSEventType type, PP_Resource resource) {
  assert(PSE_INSTANCE_HANDLEINPUT == type ||
         PSE_INSTANCE_DIDCHANGEVIEW == type);

  if (resource) {
    PSInterfaceCore()->AddRefResource(resource);
  }
  PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
  memset(env, 0, sizeof(*env));
  env->type = type;
  env->as_resource = resource;
  event_queue_.Enqueue(env);
}

void PSInstance::PostEvent(PSEventType type, const PP_Var& var) {
  assert(PSE_INSTANCE_HANDLEMESSAGE == type);

  // If the user has specified a tty_prefix_ (using ioctl), then we'll give the
  // tty node a chance to vacuum up any messages beginning with that prefix. If
  // the message does not start with the prefix, the ioctl call will return
  // ENOENT and we'll pass the message through to the event queue.
  if (fd_tty_ >= 0 && var.type == PP_VARTYPE_STRING) {
    uint32_t message_len;
    const char* message = PSInterfaceVar()->VarToUtf8(var, &message_len);
    std::string message_str(message, message + message_len);

    // Since our message may contain null characters, we can't send it as a
    // naked C string, so we package it up in this struct before sending it
    // to the ioctl.
    struct tioc_nacl_input_string ioctl_message;
    ioctl_message.length = message_len;
    ioctl_message.buffer = message_str.data();
    int ret =
      ioctl(fd_tty_, TIOCNACLINPUT, reinterpret_cast<char*>(&ioctl_message));
    if (ret != 0 && errno != ENOTTY) {
      Error("ioctl returned unexpected error: %d.\n", ret);
    }

    return;
  }

  PSInterfaceVar()->AddRef(var);
  PSEvent *env = (PSEvent *) malloc(sizeof(PSEvent));
  memset(env, 0, sizeof(*env));
  env->type = type;
  env->as_var = var;
  event_queue_.Enqueue(env);
}

PSEvent* PSInstance::TryAcquireEvent() {
  PSEvent* event;
  while(true) {
    event = event_queue_.Dequeue(false);
    if (NULL == event)
      break;
    if (events_enabled_ & event->type)
      break;
    // Release filtered events & continue to acquire.
    ReleaseEvent(event);
  }
  return event;
}

PSEvent* PSInstance::WaitAcquireEvent() {
  PSEvent* event;
  while(true) {
    event = event_queue_.Dequeue(true);
    if (events_enabled_ & event->type)
      break;
    // Release filtered events & continue to acquire.
    ReleaseEvent(event);
  }
  return event;
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
  Trace("Got Message\n");
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
