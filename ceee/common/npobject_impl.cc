// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of NPAPI object base class.
#include "ceee/common/npobject_impl.h"

void NpObjectBase::Invalidate() {
}

bool NpObjectBase::HasMethod(NPIdentifier name) {
  return false;
}

bool NpObjectBase::Invoke(NPIdentifier name,
                          const NPVariant* args,
                          uint32_t arg_count,
                          NPVariant* result) {
  return false;
}

bool NpObjectBase::InvokeDefault(const NPVariant* args,
                                uint32_t arg_count,
                                NPVariant* result) {
  return false;
}

bool NpObjectBase::HasProperty(NPIdentifier name) {
  return false;
}

bool NpObjectBase::GetProperty(NPIdentifier name, NPVariant* result) {
  return false;
}

bool NpObjectBase::SetProperty(NPIdentifier name, const NPVariant* value) {
  return false;
}

bool NpObjectBase::RemoveProperty(NPIdentifier name) {
  return false;
}

bool NpObjectBase::Enumeration(NPIdentifier** value, uint32_t* count) {
  return false;
}

bool NpObjectBase::Construct(const NPVariant* args, uint32_t argCount,
                             NPVariant* result) {
  return false;
}


void NpObjectBase::NPDeallocate(NPObject* obj) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  delete base;
}

void NpObjectBase::NPInvalidate(NPObject* obj) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  base->Invalidate();
}

bool NpObjectBase::NPHasMethod(NPObject* obj, NPIdentifier name) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->HasMethod(name);
}

bool NpObjectBase::NPInvoke(NPObject* obj,
                            NPIdentifier name,
                            const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->Invoke(name, args, arg_count, result);
}

bool NpObjectBase::NPInvokeDefault(NPObject* obj,
                            const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->InvokeDefault(args, arg_count, result);
}

bool NpObjectBase::NPHasProperty(NPObject* obj, NPIdentifier name) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->HasProperty(name);
}

bool NpObjectBase::NPGetProperty(NPObject* obj,
                                 NPIdentifier name,
                                 NPVariant* result) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->GetProperty(name, result);
}

bool NpObjectBase::NPSetProperty(NPObject* obj,
                                 NPIdentifier name,
                                 const NPVariant* value) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->SetProperty(name, value);
}

bool NpObjectBase::NPRemoveProperty(NPObject* obj,
                             NPIdentifier name) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->RemoveProperty(name);
}

bool NpObjectBase::NPEnumeration(NPObject* obj, NPIdentifier** value,
                                 uint32_t* count) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->Enumeration(value, count);
}

bool NpObjectBase::NPConstruct(NPObject* obj, const NPVariant* args,
                               uint32_t arg_count, NPVariant* result) {
  NpObjectBase* base = static_cast<NpObjectBase*>(obj);
  return base->Construct(args, arg_count, result);
}
