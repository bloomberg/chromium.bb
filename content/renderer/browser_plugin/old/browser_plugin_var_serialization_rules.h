// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_VAR_SERIALIZATION_RULES_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_VAR_SERIALIZATION_RULES_H_

#include "ppapi/proxy/var_serialization_rules.h"

namespace content {

// Implementation of the VarSerializationRules interface for the browser plugin.
class BrowserPluginVarSerializationRules :
    public ppapi::proxy::VarSerializationRules {
 public:
  explicit BrowserPluginVarSerializationRules();

  // VarSerialization implementation.
  virtual PP_Var SendCallerOwned(const PP_Var& var) OVERRIDE;
  virtual PP_Var BeginReceiveCallerOwned(const PP_Var& var) OVERRIDE;
  virtual void EndReceiveCallerOwned(const PP_Var& var) OVERRIDE;
  virtual PP_Var ReceivePassRef(const PP_Var& var) OVERRIDE;
  virtual PP_Var BeginSendPassRef(const PP_Var& var) OVERRIDE;
  virtual void EndSendPassRef(const PP_Var& var) OVERRIDE;
  virtual void ReleaseObjectRef(const PP_Var& var) OVERRIDE;

 private:
  virtual ~BrowserPluginVarSerializationRules();

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginVarSerializationRules);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_VAR_SERIALIZATION_RULES_H_
