/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "components/nacl/renderer/plugin/module_ppapi.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/utility.h"

namespace plugin {

ModulePpapi::ModulePpapi() : pp::Module(),
                             private_interface_(NULL) {
}

ModulePpapi::~ModulePpapi() {
}

bool ModulePpapi::Init() {
  // Ask the browser for an interface which provides missing functions
  private_interface_ = reinterpret_cast<const PPB_NaCl_Private*>(
      GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));

  if (NULL == private_interface_) {
    return false;
  }
  SetNaClInterface(private_interface_);

  return true;
}

pp::Instance* ModulePpapi::CreateInstance(PP_Instance pp_instance) {
  return new Plugin(pp_instance);
}

}  // namespace plugin


namespace pp {

Module* CreateModule() {
  return new plugin::ModulePpapi();
}

}  // namespace pp
