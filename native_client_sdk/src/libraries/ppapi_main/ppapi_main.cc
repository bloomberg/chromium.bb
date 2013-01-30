/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

#include "ppapi_main/ppapi_instance.h"
#include "ppapi_main/ppapi_instance3d.h"
#include "ppapi_main/ppapi_main.h"


static pp::Instance* s_Instance = NULL;


// Helpers to access PPAPI interfaces
void* PPAPI_GetInstanceObject() {
  return s_Instance;
}

PP_Instance PPAPI_GetInstanceId() {
  return s_Instance->pp_instance();
}

const void* PPAPI_GetInterface(const char *name) {
  return pp::Module::Get()->GetBrowserInterface(name);
}

void* PPAPI_CreateInstance(PP_Instance inst, const char *args[]) {
  return static_cast<void*>(new PPAPIInstance(inst, args));
}

PPAPIEvent* PPAPI_AcquireEvent() {
  return static_cast<PPAPIInstance*>(s_Instance)->AcquireInputEvent();
}

void PPAPI_ReleaseEvent(PPAPIEvent* event) {
  static_cast<PPAPIInstance*>(s_Instance)->ReleaseInputEvent(event);
}


/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class PPAPIMainModule : public pp::Module {
 public:
  PPAPIMainModule() : pp::Module() {}
  virtual ~PPAPIMainModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    s_Instance = static_cast<pp::Instance*>(UserCreateInstance(instance));
    return s_Instance;
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new PPAPIMainModule();
}

}  // namespace pp

