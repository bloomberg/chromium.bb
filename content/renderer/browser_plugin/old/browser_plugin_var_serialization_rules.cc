// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/old/browser_plugin_var_serialization_rules.h"

#include "base/logging.h"

namespace content {

BrowserPluginVarSerializationRules::BrowserPluginVarSerializationRules() {
}

BrowserPluginVarSerializationRules::~BrowserPluginVarSerializationRules() {
}

PP_Var BrowserPluginVarSerializationRules::SendCallerOwned(const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
  return var;
}

PP_Var BrowserPluginVarSerializationRules::BeginReceiveCallerOwned(
    const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
  return var;
}

void BrowserPluginVarSerializationRules::EndReceiveCallerOwned(
    const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
}

PP_Var BrowserPluginVarSerializationRules::ReceivePassRef(const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
  return var;
}

PP_Var BrowserPluginVarSerializationRules::BeginSendPassRef(const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
  return var;
}

void BrowserPluginVarSerializationRules::EndSendPassRef(const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
}

void BrowserPluginVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  DCHECK(var.type != PP_VARTYPE_OBJECT);
}

}  // namespace content
