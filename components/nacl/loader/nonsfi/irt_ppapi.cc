// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "ppapi/c/ppp.h"
#include "ppapi/nacl_irt/plugin_main.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"

namespace nacl {
namespace nonsfi {
namespace {

struct PP_StartFunctions g_pp_functions;

int IrtPpapiStart(const struct PP_StartFunctions* funcs) {
  g_pp_functions = *funcs;
  return PpapiPluginMain();
}

}  // namespace

const struct nacl_irt_ppapihook kIrtPpapiHook = {
  IrtPpapiStart,
  PpapiPluginRegisterThreadCreator,
};

}  // namespace nonsfi
}  // namespace nacl

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  return nacl::nonsfi::g_pp_functions.PPP_InitializeModule(
      module_id, get_browser_interface);
}

void PPP_ShutdownModule(void) {
  nacl::nonsfi::g_pp_functions.PPP_ShutdownModule();
}

const void *PPP_GetInterface(const char *interface_name) {
  return nacl::nonsfi::g_pp_functions.PPP_GetInterface(interface_name);
}
