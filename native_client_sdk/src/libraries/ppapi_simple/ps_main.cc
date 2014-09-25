// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef __native_client__
#include <irt.h>
#include <irt_ppapi.h>
#endif

#include <stdio.h>

#include "nacl_io/nacl_io.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_main.h"

extern "C" int PpapiPluginMain();

void* PSMainCreate(PP_Instance inst, PSMainFunc_t entry_point) {
  PSInstance* pInst = new PSInstance(inst);
  pInst->SetMain(entry_point);
  return pInst;
}

/**
 * main entry point for ppapi_simple applications.  This differs from the
 * regular ppapi main entry point in that it will fall back to running
 * the user's main code in the case that the PPAPI hooks are not found.
 * This allows ppapi_simple binary to run within chrome (with PPAPI present)
 * and also under sel_ldr (no PPAPI).
 */
#ifdef __native_client__
extern "C" int __nacl_main(int argc, char* argv[]) {
  struct nacl_irt_ppapihook hooks;
  if (nacl_interface_query(NACL_IRT_PPAPIHOOK_v0_1, &hooks, sizeof(hooks)) ==
      sizeof(hooks)) {
    return PpapiPluginMain();
  }
#else
int main(int argc, char* argv[]) {
#endif
  // By default, or if not running in the browser we simply run the main
  // entry point directly, on the main thread.
  nacl_io_init();
  int rtn = PSUserMainGet()(argc, argv);
  nacl_io_uninit();
  return rtn;
}
