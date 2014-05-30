// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"
#include "ui/accessibility/ax_enums.h"

namespace {

// Helper to convert an enum to a V8 object.
template <typename EnumType>
v8::Local<v8::Object> ToEnumObject(v8::Isolate* isolate,
                                   EnumType start_after,
                                   EnumType end_at) {
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  for (int i = start_after + 1; i <= end_at; ++i) {
    v8::Local<v8::String> value = v8::String::NewFromUtf8(
        isolate, ui::ToString(static_cast<EnumType>(i)).c_str());
    object->Set(value, value);
  }
  return object;
}

}  // namespace

namespace extensions {

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context) : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "IsInteractPermitted",
      base::Bind(&AutomationInternalCustomBindings::IsInteractPermitted,
                 base::Unretained(this)));
  RouteFunction(
      "GetSchemaAdditions",
      base::Bind(&AutomationInternalCustomBindings::GetSchemaAdditions,
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

void AutomationInternalCustomBindings::GetSchemaAdditions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Object> additions = v8::Object::New(GetIsolate());

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "EventType"),
      ToEnumObject(GetIsolate(), ui::AX_EVENT_NONE, ui::AX_EVENT_LAST));

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "RoleType"),
      ToEnumObject(GetIsolate(), ui::AX_ROLE_NONE, ui::AX_ROLE_LAST));

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "StateType"),
      ToEnumObject(GetIsolate(), ui::AX_STATE_NONE, ui::AX_STATE_LAST));

  args.GetReturnValue().Set(additions);
}

}  // namespace extensions
