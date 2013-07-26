// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"

#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_main.h"


void* PSMainCreate(PP_Instance inst, PSMainFunc_t func) {
  PSInstance* pInst = new PSInstance(inst);
  pInst->SetMain(func);
  return pInst;
}
