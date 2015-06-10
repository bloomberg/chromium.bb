// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"
#include "ipc/message_filter.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"

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

class AutomationMessageFilter : public IPC::MessageFilter {
 public:
  explicit AutomationMessageFilter(AutomationInternalCustomBindings* owner)
      : owner_(owner),
        removed_(false) {
    DCHECK(owner);
    content::RenderThread::Get()->AddFilter(this);
  }

  void Detach() {
    owner_ = nullptr;
    Remove();
  }

  // IPC::MessageFilter
  bool OnMessageReceived(const IPC::Message& message) override {
    if (owner_)
      return owner_->OnMessageReceived(message);
    else
      return false;
  }

  void OnFilterRemoved() override {
    removed_ = true;
  }

private:
  ~AutomationMessageFilter() override {
    Remove();
  }

  void Remove() {
    if (!removed_) {
      removed_ = true;
      content::RenderThread::Get()->RemoveFilter(this);
    }
  }

  AutomationInternalCustomBindings* owner_;
  bool removed_;

  DISALLOW_COPY_AND_ASSIGN(AutomationMessageFilter);
};

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context) : ObjectBackedNativeHandler(context) {
  // It's safe to use base::Unretained(this) here because these bindings
  // will only be called on a valid AutomationInternalCustomBindings instance
  // and none of the functions have any side effects.
  RouteFunction(
      "IsInteractPermitted",
      base::Bind(&AutomationInternalCustomBindings::IsInteractPermitted,
                 base::Unretained(this)));
  RouteFunction(
      "GetSchemaAdditions",
      base::Bind(&AutomationInternalCustomBindings::GetSchemaAdditions,
                 base::Unretained(this)));
  RouteFunction(
      "GetRoutingID",
      base::Bind(&AutomationInternalCustomBindings::GetRoutingID,
                 base::Unretained(this)));

  message_filter_ = new AutomationMessageFilter(this);
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {
  message_filter_->Detach();
}

bool AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEvent, OnAccessibilityEvent)
  IPC_END_MESSAGE_MAP()

  // Always return false in case there are multiple
  // AutomationInternalCustomBindings instances attached to the same thread.
  return false;
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

void AutomationInternalCustomBindings::GetRoutingID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  int routing_id = context()->GetRenderView()->GetRoutingID();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), routing_id));
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

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "TreeChangeType"),
      ToEnumObject(GetIsolate(), ui::AX_MUTATION_NONE, ui::AX_MUTATION_LAST));

  args.GetReturnValue().Set(additions);
}

void AutomationInternalCustomBindings::OnAccessibilityEvent(
    const ExtensionMsg_AccessibilityEventParams& params) {
  // TODO(dmazzoni): finish implementing this.
}

}  // namespace extensions
