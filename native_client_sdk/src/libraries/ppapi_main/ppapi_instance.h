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

#include "ppapi_main/ppapi_event.h"
#include "ppapi_main/ppapi_queue.h"

typedef std::map<std::string, std::string> PropertyMap_t;

class PPAPIInstance : public pp::Instance {
 public:
  PPAPIInstance(PP_Instance instance, const char *args[]);
  virtual ~PPAPIInstance();

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

  // Accessors for the PPAPIQueue object.  Use Acquire to fetch the top
  // event if one is available, then release when done.  Events must be
  // acquired and released one at a time.
  virtual PPAPIEvent* AcquireInputEvent();
  virtual void ReleaseInputEvent(PPAPIEvent* event);

  static PPAPIInstance* GetInstance();

 protected:
  // Called to launch ppapi_main
  static void* StartMain(void *start_info);

  // Called by Init to processes default and embed tag arguments prior to
  // launching the 'ppapi_main' thread.
  virtual bool ProcessProperties();

  // Returns value based on KEY or default
  const char* GetProperty(const char* key, const char* def = NULL);

 private:
  pp::MessageLoop main_loop_;
  PropertyMap_t properties_;
  PPAPIQueue event_queue_;
  bool has_focus_;
};


#endif  // PPAPI_MAIN_PPAPI_INSTANCE_H_
