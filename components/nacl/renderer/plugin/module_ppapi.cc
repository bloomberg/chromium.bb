/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "components/nacl/renderer/plugin/module_ppapi.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"

namespace plugin {

ModulePpapi::ModulePpapi() : pp::Module(),
                             init_was_successful_(false),
                             private_interface_(NULL) {
}

ModulePpapi::~ModulePpapi() {
  if (init_was_successful_) {
    NaClNrdAllModulesFini();
  }
}

bool ModulePpapi::Init() {
  // Ask the browser for an interface which provides missing functions
  private_interface_ = reinterpret_cast<const PPB_NaCl_Private*>(
      GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));

  if (NULL == private_interface_) {
    return false;
  }
  SetNaClInterface(private_interface_);

#if NACL_LINUX || NACL_OSX
  // Note that currently we do not need random numbers inside the
  // NaCl trusted plugin on Unix, but NaClSecureRngModuleInit() is
  // strict and will raise a fatal error unless we provide it with a
  // /dev/urandom FD beforehand.
  NaClSecureRngModuleSetUrandomFd(dup(private_interface_->UrandomFD()));
#endif

  // In the plugin, we don't need high resolution time of day.
  NaClAllowLowResolutionTimeOfDay();
  NaClNrdAllModulesInit();

  init_was_successful_ = true;
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
