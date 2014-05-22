// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context) : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "IsInteractPermitted",
      base::Bind(&AutomationInternalCustomBindings::IsInteractPermitted,
                 base::Unretained(this)));
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {
}

void AutomationInternalCustomBindings::IsInteractPermitted(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  const Extension* extension = context()->extension();
  CHECK(extension);
  const AutomationInfo* automation_info = AutomationInfo::Get(extension);
  CHECK(automation_info);
  args.GetReturnValue().Set(
      v8::Boolean::New(GetIsolate(), automation_info->interact));
}

}  // namespace extensions
