// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_INSTANCE_H_
#define PPAPI_MAIN_PPAPI_INSTANCE_H_

#include <map>

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/mouse_lock.h"

#include "ppapi/utility/completion_callback_factory.h"

#include "ppapi_main/ppapi_event.h"
#include "ppapi_main/ppapi_queue.h"


typedef std::map<std::string, std::string> PropertyMap_t;

class PPAPIInstance : public pp::Instance {
 public:
  PPAPIInstance(PP_Instance instance, const char *args[]);
  virtual ~PPAPIInstance();

  //
  // Callback functions triggered by Pepepr
  //

  // Called by the browser when the NaCl module is loaded and all ready to go.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Called whenever the in-browser window changes size.
  virtual void DidChangeView(const pp::View& view);

  // Called by the browser when the NaCl canvas gets or loses focus.
  virtual void DidChangeFocus(bool has_focus);

  // Called by the browser to handle the postMessage() call in Javascript.
  virtual void HandleMessage(const pp::Var& message);

  // Called by the browser to handle incoming input events.
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  // Called when the graphics flush completes
  virtual void Flushed(int result);

  // Called when we need to rebuild the Graphics device context.  This usually
  // happens as a result of DidChangeView.
  virtual void BuildContext(int32_t result, const pp::Size& new_size);

  // Called with the current graphics context, current size and width, when
  // the application is ready to render, either due to a newly build
  // Context, or a successful Flush of the previous frame.
  virtual void Render(PP_Resource ctx, uint32_t width, uint32_t height);

  //
  // Thread entry points
  //
  virtual int MainThread(int argc, const char* argv[]);
  virtual void* EventThread();

  //
  // Request API
  //
  bool ToggleFullscreen();
  virtual PPAPIEvent* AcquireInputEvent();
  virtual void ReleaseInputEvent(PPAPIEvent* event);
  static PPAPIInstance* GetInstance();

 protected:
  // Called to run ppapi_main.
  static void* MainThreadThunk(void *start_info);

  // Called if message processing and rendering happens off the main thread.
  static void* EventThreadThunk(void *this_ptr);

  // Called by Init to processes default and embed tag arguments prior to
  // launching the 'ppapi_main' thread.
  virtual bool ProcessProperties();

  // Returns value based on KEY or default.
  const char* GetProperty(const char* key, const char* def = NULL);

 protected:
  pp::MessageLoop main_loop_;
  pp::Fullscreen fullscreen_;
  pp::MessageLoop render_loop_;
  pp::CompletionCallbackFactory<PPAPIInstance> callback_factory_;

  PropertyMap_t properties_;
  PPAPIQueue event_queue_;

  pp::Size size_;
  bool has_focus_;
  bool is_context_bound_;
  bool was_fullscreen_;
  bool mouse_locked_;
  bool use_main_thread_;
};


#endif  // PPAPI_MAIN_PPAPI_INSTANCE_H_
