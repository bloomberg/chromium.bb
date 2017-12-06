// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/runtime_hooks_delegate.h"

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"

namespace extensions {

namespace {
using RequestResult = APIBindingHooks::RequestResult;

// Handler for the extensionId property on chrome.runtime.
void GetExtensionId(v8::Local<v8::Name> property_name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  // This could potentially be invoked after the script context is removed
  // (unlike the handler calls, which should only be invoked for valid
  // contexts).
  if (script_context && script_context->extension()) {
    info.GetReturnValue().Set(
        gin::StringToSymbol(isolate, script_context->extension()->id()));
  }
}

constexpr char kGetManifest[] = "runtime.getManifest";
constexpr char kGetURL[] = "runtime.getURL";
constexpr char kConnect[] = "runtime.connect";
constexpr char kConnectNative[] = "runtime.connectNative";
constexpr char kSendMessage[] = "runtime.sendMessage";
constexpr char kSendNativeMessage[] = "runtime.sendNativeMessage";

}  // namespace

RuntimeHooksDelegate::RuntimeHooksDelegate(
    NativeRendererMessagingService* messaging_service)
    : messaging_service_(messaging_service) {}
RuntimeHooksDelegate::~RuntimeHooksDelegate() {}

RequestResult RuntimeHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (RuntimeHooksDelegate::*)(
      ScriptContext*, const std::vector<v8::Local<v8::Value>>&);
  static const struct {
    Handler handler;
    base::StringPiece method;
  } kHandlers[] = {
      {&RuntimeHooksDelegate::HandleSendMessage, kSendMessage},
      {&RuntimeHooksDelegate::HandleConnect, kConnect},
      {&RuntimeHooksDelegate::HandleGetURL, kGetURL},
      {&RuntimeHooksDelegate::HandleGetManifest, kGetManifest},
      {&RuntimeHooksDelegate::HandleConnectNative, kConnectNative},
      {&RuntimeHooksDelegate::HandleSendNativeMessage, kSendNativeMessage},
  };

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  if (method_name == kSendMessage) {
    messaging_util::MassageSendMessageArguments(context->GetIsolate(), true,
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

void RuntimeHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  object_template->SetAccessor(gin::StringToSymbol(isolate, "id"),
                               &GetExtensionId);
}

RequestResult RuntimeHooksDelegate::HandleGetManifest(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& parsed_arguments) {
  DCHECK(script_context->extension());

  RequestResult result(RequestResult::HANDLED);
  result.return_value = content::V8ValueConverter::Create()->ToV8Value(
      script_context->extension()->manifest()->value(),
      script_context->v8_context());

  return result;
}

RequestResult RuntimeHooksDelegate::HandleGetURL(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  DCHECK(script_context->extension());

  std::string path = gin::V8ToString(arguments[0]);

  RequestResult result(RequestResult::HANDLED);
  std::string url = base::StringPrintf(
      "chrome-extension://%s%s%s", script_context->extension()->id().c_str(),
      !path.empty() && path[0] == '/' ? "" : "/", path.c_str());
  result.return_value = gin::StringToV8(script_context->isolate(), url);

  return result;
}

RequestResult RuntimeHooksDelegate::HandleSendMessage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(4u, arguments.size());

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "runtime.sendMessage", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Context> v8_context = script_context->v8_context();
  messaging_util::MessageOptions options;
  if (!arguments[2]->IsNull()) {
    options = messaging_util::ParseMessageOptions(
        v8_context, arguments[2].As<v8::Object>(),
        messaging_util::PARSE_INCLUDE_TLS_CHANNEL_ID);
  }

  v8::Local<v8::Value> v8_message = arguments[1];
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(v8_context, v8_message, &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[3]->IsNull())
    response_callback = arguments[3].As<v8::Function>();

  messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForExtension(target_id),
      messaging_util::kSendMessageChannel, options.include_tls_channel_id,
      *message, response_callback);

  return RequestResult(RequestResult::HANDLED);
}

RequestResult RuntimeHooksDelegate::HandleSendNativeMessage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(3u, arguments.size());

  std::string application_name = gin::V8ToString(arguments[0]);

  v8::Local<v8::Value> v8_message = arguments[1];
  DCHECK(!v8_message.IsEmpty());
  std::string error;
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
      script_context, MessageTarget::ForNativeApp(application_name),
      std::string(), false, *message, response_callback);

  return RequestResult(RequestResult::HANDLED);
}

RequestResult RuntimeHooksDelegate::HandleConnect(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(2u, arguments.size());

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "runtime.connect", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  messaging_util::MessageOptions options;
  if (!arguments[1]->IsNull()) {
    options = messaging_util::ParseMessageOptions(
        script_context->v8_context(), arguments[1].As<v8::Object>(),
        messaging_util::PARSE_INCLUDE_TLS_CHANNEL_ID |
            messaging_util::PARSE_CHANNEL_NAME);
  }

  gin::Handle<GinPort> port = messaging_service_->Connect(
      script_context, MessageTarget::ForExtension(target_id),
      options.channel_name, options.include_tls_channel_id);
  DCHECK(!port.IsEmpty());

  RequestResult result(RequestResult::HANDLED);
  result.return_value = port.ToV8();
  return result;
}

RequestResult RuntimeHooksDelegate::HandleConnectNative(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());

  std::string application_name = gin::V8ToString(arguments[0]);
  gin::Handle<GinPort> port = messaging_service_->Connect(
      script_context, MessageTarget::ForNativeApp(application_name),
      std::string(), false);

  RequestResult result(RequestResult::HANDLED);
  result.return_value = port.ToV8();
  return result;
}

}  // namespace extensions
