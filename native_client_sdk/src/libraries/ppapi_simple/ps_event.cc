// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_main.h"


void PSEventPost(PSEventType type) {
  PSInstance::GetInstance()->PostEvent(type);
}

void PSEventPostBool(PSEventType type, PP_Bool state) {
  PSInstance::GetInstance()->PostEvent(type, state);
}

void PSEventPostVar(PSEventType type, struct PP_Var var) {
  PSInstance::GetInstance()->PostEvent(type, var);
}

void PSEventPostResource(PSEventType type, PP_Resource resource) {
  PSInstance::GetInstance()->PostEvent(type, resource);
}

PSEvent* PSEventTryAcquire() {
  return PSInstance::GetInstance()->TryAcquireEvent();
}

PSEvent* PSEventWaitAcquire() {
  return PSInstance::GetInstance()->WaitAcquireEvent();
}

void PSEventRelease(PSEvent* event) {
  PSInstance::GetInstance()->ReleaseEvent(event);
}

void PSEventSetFilter(PSEventTypeMask filter) {
  PSInstance::GetInstance()->SetEnabledEvents(filter);
}

