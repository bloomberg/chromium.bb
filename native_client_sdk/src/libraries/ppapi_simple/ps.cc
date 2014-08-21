// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

#include "ppapi_simple/ps.h"

static pp::Instance* s_Instance = NULL;

PP_Instance PSGetInstanceId(void) {
  if (s_Instance == NULL)
    return 0;
  return s_Instance->pp_instance();
}

const void* PSGetInterface(const char *name) {
  pp::Module* module = pp::Module::Get();
  if (module == NULL)
    return NULL;
  return module->GetBrowserInterface(name);
}


// The Module class.  The browser calls the CreateInstance() method to create
// an instance of your NaCl module on the web page.  The browser creates a new
// instance for each <embed> tag with type="application/x-nacl".
class PSModule : public pp::Module {
 public:
  PSModule() : pp::Module() {}
  virtual ~PSModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    s_Instance = static_cast<pp::Instance*>(PSUserCreateInstance(instance));
    return s_Instance;
  }
};

namespace pp {

// Factory function called by the browser when the module is first loaded.
// The browser keeps a singleton of this module.  It calls the
// CreateInstance() method on the object you return to make instances.  There
// is one instance per <embed> tag on the page.  This is the main binding
// point for your NaCl module with the browser.
Module* CreateModule() {
  return new PSModule();
}

}  // namespace pp

