// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_INSTANCE_H_
#define PPAPI_MAIN_PPAPI_INSTANCE_H_

#include <map>

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/instance.h"

typedef std::map<std::string, std::string> PropteryMap_t;

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

 protected:
  // Called by Init to processes default and embed tag arguments prior to
  // launching the 'ppapi_main' thread.
  virtual bool ProcessProperties();

  // Returns value based on KEY or default
  const char* GetProperty(const char* key, const char* def = NULL);

 private:
  PropteryMap_t properties_;
  bool has_focus_;
};


#endif  // PPAPI_MAIN_PPAPI_INSTANCE_H_
