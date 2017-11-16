// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_hooks_delegate.h"

#include "base/strings/stringprintf.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"

namespace extensions {

namespace {

using RequestResult = APIBindingHooks::RequestResult;

constexpr char kSendRequest[] = "extension.sendRequest";

// We alias a bunch of chrome.extension APIs to their chrome.runtime
// counterparts.
// NOTE(devlin): This is a very simple alias, in which we just return the
// runtime version from the chrome.runtime object. This is important to note
// for a few reasons:
// - Modifications to the chrome.runtime object will affect the return result
//   here. i.e., if script does chrome.runtime.sendMessage = 'some string',
//   then chrome.extension.sendMessage will also be 'some string'.
// - Events will share listeners. i.e., a listener added to
//   chrome.extension.onMessage will fire from a runtime.onMessage event, and
//   vice versa.
// All of these APIs have been deprecated, and are no longer even documented,
// but still have usage. This is the cheap workaround that JS bindings have been
// using, and, while not robust, it should be secure, so use it native bindings,
// too.
void GetAliasedFeature(v8::Local<v8::Name> property_name,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> chrome;
  if (!context->Global()
           ->Get(context, gin::StringToSymbol(isolate, "chrome"))
           .ToLocal(&chrome) ||
      !chrome->IsObject()) {
    return;
  }

  v8::Local<v8::Value> runtime;
  if (!chrome.As<v8::Object>()
           ->Get(context, gin::StringToSymbol(isolate, "runtime"))
           .ToLocal(&runtime) ||
      !runtime->IsObject()) {
    return;
  }

  v8::Local<v8::Object> runtime_obj = runtime.As<v8::Object>();
  v8::Maybe<bool> has_property =
      runtime_obj->HasRealNamedProperty(context, property_name);
  if (!has_property.IsJust() || !has_property.FromJust())
    return;

  info.GetReturnValue().Set(
      runtime_obj->Get(context, property_name).ToLocalChecked());
}

}  // namespace

ExtensionHooksDelegate::ExtensionHooksDelegate(
    NativeRendererMessagingService* messaging_service)
    : messaging_service_(messaging_service) {}
ExtensionHooksDelegate::~ExtensionHooksDelegate() {}

RequestResult ExtensionHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  // TODO(devlin): This logic is the same in the RuntimeCustomHooksDelegate -
  // would it make sense to share it?
  using Handler = RequestResult (ExtensionHooksDelegate::*)(
      ScriptContext*, const std::vector<v8::Local<v8::Value>>&);
  static struct {
    Handler handler;
    base::StringPiece method;
  } kHandlers[] = {
      {&ExtensionHooksDelegate::HandleSendRequest, kSendRequest},
      // TODO(devlin): Add getBackgroundPage, getExtensionTabs, getViews.
  };

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  DCHECK(script_context);

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  if (method_name == kSendRequest) {
    messaging_util::MassageSendMessageArguments(context->GetIsolate(), false,
                                                arguments);
  }

  std::string error;
  std::vector<v8::Local<v8::Value>> parsed_arguments;
  if (!signature->ParseArgumentsToV8(context, *arguments, refs,
                                     &parsed_arguments, &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  return (this->*handler)(script_context, parsed_arguments);
}

void ExtensionHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  static const char* const kAliases[] = {
      "connect",   "connectNative",     "sendMessage", "sendNativeMessage",
      "onConnect", "onConnectExternal", "onMessage",   "onMessageExternal",
  };

  for (const auto* alias : kAliases) {
    object_template->SetAccessor(gin::StringToSymbol(isolate, alias),
                                 &GetAliasedFeature);
  }
}

RequestResult ExtensionHooksDelegate::HandleSendRequest(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(3u, arguments.size());

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "extension.sendRequest", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Value> v8_message = arguments[1];
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      script_context->v8_context(), v8_message, &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[2]->IsNull())
    response_callback = arguments[2].As<v8::Function>();

  messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForExtension(target_id),
      messaging_util::kSendRequestChannel, false, *message, response_callback);

  return RequestResult(RequestResult::HANDLED);
}

}  // namespace extensions
