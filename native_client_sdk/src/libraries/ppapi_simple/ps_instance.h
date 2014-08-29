// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SIMPLE_PS_INSTANCE_H_
#define PPAPI_SIMPLE_PS_INSTANCE_H_

#include <stdarg.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_view.h"

#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/mouse_lock.h"

#include "ppapi/utility/completion_callback_factory.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_main.h"

#include "sdk_util/thread_safe_queue.h"

typedef void (*MessageHandler_t)(const pp::Var& key,
                                 const pp::Var& value,
                                 void* user_data);

struct MessageHandler {
  MessageHandler_t handler;
  void* user_data;
};

// The basic instance class which also inherits the MouseLock and
// Graphics3DClient interfaces.
class PSInstance : public pp::Instance, pp::MouseLock, pp::Graphics3DClient {
 public:
  // Verbosity levels, ecplicitly numbered since we pass these
  // in from html attributes as numberic values.
  enum Verbosity {
    PSV_SILENT = 0,
    PSV_ERROR = 1,
    PSV_WARN = 2,
    PSV_LOG = 3,
    PSV_TRACE = 4,
  };

  // Returns a pointer to the global instance
  static PSInstance* GetInstance();

  PSInstance(PP_Instance inst);
  virtual ~PSInstance();

  // Set a function which will be called on a new thread once initialized.
  // NOTE:  This must be called by the Factory, once Init is called, this
  // function will have no effect.
  void SetMain(PSMainFunc_t func);

  // Started on Init, a thread which can be safely blocked.
  virtual int MainThread(int argc, char* argv[]);

  // Logging Functions
  void SetVerbosity(Verbosity verbosity);
  void Trace(const char *fmt, ...);
  void Log(const char *fmt, ...);
  void Warn(const char *fmt, ...);
  void Error(const char *fmt, ...);

  // Event Functions
  void SetEnabledEvents(uint32_t mask);
  void PostEvent(PSEventType type);
  void PostEvent(PSEventType type, PP_Bool bool_value);
  void PostEvent(PSEventType type, PP_Resource resource);
  void PostEvent(PSEventType type, const PP_Var& var);

  PSEvent* TryAcquireEvent();
  PSEvent* WaitAcquireEvent();
  void ReleaseEvent(PSEvent* event);

  // Register a message handler for messages that arrive
  // from JavaScript with a give names.  Messages are of the
  // form:  { message_name : <value> }.
  //
  // PSInstance will then not generate events but instead
  // cause the handler to be called upon message arrival.
  // If handler is NULL then the current handler will be
  // removed. Example usage:
  //
  // JavaScript:
  //   nacl_module.postMessage({'foo': 123});
  //
  // C++:
  //   void MyMessageHandler(const pp::Var& key,
  //                         const pp::Var& value,
  //                         void* user_data) {
  //     assert(key.is_string());
  //     assert(key.AsString() == "foo");
  //     assert(value.is_int());
  //     assert(value.AsInt() == 123);
  //   }
  //   ...
  //   instance_->RegisterMessageHandler("foo", &MyMessageHandler, NULL);
  //
  void RegisterMessageHandler(std::string message_name,
                              MessageHandler_t handler,
                              void* user_data);

  // Perform exit handshake with JavaScript.
  // This is called by _exit before the process is terminated to ensure
  // that all messages sent prior to _exit arrive at the JavaScript side.
  void ExitHandshake(int status);

 protected:
  typedef std::map<std::string, MessageHandler> MessageHandlerMap;

  // Callback functions triggered by Pepper
  //
  // These functions are called on the main pepper thread, so they must
  // not block.
  //
  // Called by the browser when the NaCl module is loaded and all ready to go.
  // This function will create a new thread which will run the pseudo main.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Output log message to stderr if the current verbosity is set
  // at or above the given verbosity.
  void VALog(Verbosity verbosity, const char *fmt, va_list args);

  // Called whenever the in-browser window changes size, it will pass a
  // context change request to whichever thread is handling rendering.
  virtual void DidChangeView(const pp::View& view);

  // Called by the browser when the NaCl canvas gets or loses focus.
  virtual void DidChangeFocus(bool has_focus);

  // Called by the browser to handle the postMessage() call in Javascript.
  virtual void HandleMessage(const pp::Var& message);

  // Called by the browser to handle incoming input events.  Events are Q'd
  // and can later be processed on a sperate processing thread.
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  // Called by the browser when the 3D context is lost.
  virtual void Graphics3DContextLost();

  // Called by the browser when the mouselock is lost.
  virtual void MouseLockLost();

  // Called by Init to processes default and embed tag arguments prior to
  // launching the 'ppapi_main' thread.
  virtual bool ProcessProperties();

 private:
  static void* MainThreadThunk(void *start_info);
  ssize_t TtyOutputHandler(const char* buf, size_t count);
  void MessageHandlerExit(const pp::Var& message);
  void MessageHandlerInput(const pp::Var& key, const pp::Var& message);
  void MessageHandlerResize(const pp::Var& message);
  void HandleResize(int width, int height);

  static void HandleExitStatic(int status, void* user_data);

  static ssize_t TtyOutputHandlerStatic(const char* buf, size_t count,
                                        void* user_data);

  /// Handle exit confirmation message from JavaScript.
  static void MessageHandlerExitStatic(const pp::Var& key,
                                       const pp::Var& message,
                                       void* user_data);

  /// Handle input message from JavaScript.  The value is
  /// expected to be of type string.
  static void MessageHandlerInputStatic(const pp::Var& key,
                                        const pp::Var& message,
                                        void* user_data);


  /// Handle resizs message from JavaScript.  The value is
  /// expected to be an array of 2 integers representing the
  /// number of columns and rows in the TTY.
  static void MessageHandlerResizeStatic(const pp::Var& key,
                                         const pp::Var& message,
                                         void* user_data);

 protected:
  pp::MessageLoop* main_loop_;

  sdk_util::ThreadSafeQueue<PSEvent> event_queue_;
  uint32_t events_enabled_;
  Verbosity verbosity_;

  // TTY handling
  int tty_fd_;
  const char* tty_prefix_;
  MessageHandlerMap message_handlers_;

  PSMainFunc_t main_cb_;

  const PPB_Core* ppb_core_;
  const PPB_Var* ppb_var_;
  const PPB_View* ppb_view_;

  // Condition variable and lock used to wait for exit confirmation from
  // JavaScript.
  pthread_cond_t exit_cond_;
  pthread_mutex_t exit_lock_;

  // A message to Post to JavaScript instead of exiting, or NULL if exit()
  // should be called instead.
  char* exit_message_;
};

#endif  // PPAPI_SIMPLE_PS_INSTANCE_H_
