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
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"
#include "gin/dictionary.h"

namespace extensions {

namespace {
using RequestResult = APIBindingHooks::RequestResult;

constexpr char kExtensionIdRequiredErrorTemplate[] =
    "chrome.runtime.%s() called from a webpage must "
    "specify an Extension ID (string) for its first argument.";

// Parses the target from |v8_target_id|, or uses the extension associated with
// the |script_context| as a default. Returns true on success, and false on
// failure.
bool GetTarget(ScriptContext* script_context,
               v8::Local<v8::Value> v8_target_id,
               std::string* target_out) {
  DCHECK(!v8_target_id.IsEmpty());

  std::string target_id;
  if (v8_target_id->IsNull()) {
    if (!script_context->extension())
      return false;

    *target_out = script_context->extension()->id();
  } else {
    DCHECK(v8_target_id->IsString());
    *target_out = gin::V8ToString(v8_target_id);
  }

  return true;
}

// The result of trying to parse options passed to a messaging API.
enum ParseOptionsResult {
  TYPE_ERROR,  // Invalid values were passed.
  THROWN,      // An error was thrown while parsing.
  SUCCESS,     // Parsing succeeded.
};

struct MessageOptions {
  std::string channel_name;
  bool include_tls_channel_id = false;
};

// Parses the parameters sent to sendMessage or connect, returning the result of
// the attempted parse. If |check_for_channel_name| is true, also checks for a
// provided channel name (this is only true for connect() calls). Populates the
// result in |options_out| or |error_out| (depending on the success of the
// parse).
ParseOptionsResult ParseMessageOptions(v8::Local<v8::Context> context,
                                       v8::Local<v8::Object> v8_options,
                                       bool check_for_channel_name,
                                       MessageOptions* options_out,
                                       std::string* error_out) {
  DCHECK(!v8_options.IsEmpty());
  DCHECK(!v8_options->IsNull());

  v8::Isolate* isolate = context->GetIsolate();

  MessageOptions options;

  // Theoretically, our argument matching code already checked the types of
  // the properties on v8_connect_options. However, since we don't make an
  // independent copy, it's possible that author script has super sneaky
  // getters/setters that change the result each time the property is
  // queried. Make no assumptions.
  v8::Local<v8::Value> v8_channel_name;
  v8::Local<v8::Value> v8_include_tls_channel_id;
  gin::Dictionary options_dict(isolate, v8_options);
  if (!options_dict.Get("includeTlsChannelId", &v8_include_tls_channel_id) ||
      (check_for_channel_name && !options_dict.Get("name", &v8_channel_name))) {
    return THROWN;
  }

  if (check_for_channel_name && !v8_channel_name->IsUndefined()) {
    if (!v8_channel_name->IsString()) {
      *error_out = "connectInfo.name must be a string.";
      return TYPE_ERROR;
    }
    options.channel_name = gin::V8ToString(v8_channel_name);
  }

  if (!v8_include_tls_channel_id->IsUndefined()) {
    if (!v8_include_tls_channel_id->IsBoolean()) {
      *error_out = "connectInfo.includeTlsChannelId must be a boolean.";
      return TYPE_ERROR;
    }
    options.include_tls_channel_id = v8_include_tls_channel_id->BooleanValue();
  }

  *options_out = std::move(options);
  return SUCCESS;
}

// Massages the sendMessage() arguments into the expected schema. These
// arguments are ambiguous (could match multiple signatures), so we can't just
// rely on the normal signature parsing. Sets |arguments| to the result if
// successful; otherwise leaves |arguments| untouched. (If the massage is
// unsuccessful, our normal argument parsing code should throw a reasonable
// error.
void MassageSendMessageArguments(
    v8::Isolate* isolate,
    std::vector<v8::Local<v8::Value>>* arguments_out) {
  base::span<const v8::Local<v8::Value>> arguments = *arguments_out;
  if (arguments.empty() || arguments.size() > 4u)
    return;

  v8::Local<v8::Value> target_id = v8::Null(isolate);
  v8::Local<v8::Value> message = v8::Null(isolate);
  v8::Local<v8::Value> options = v8::Null(isolate);
  v8::Local<v8::Value> response_callback = v8::Null(isolate);

  // If the last argument is a function, it is the response callback.
  // Ignore it for the purposes of further argument parsing.
  if ((*arguments.rbegin())->IsFunction()) {
    response_callback = *arguments.rbegin();
    arguments = arguments.first(arguments.size() - 1);
  }

  switch (arguments.size()) {
    case 0:
      // Required argument (message) is missing.
      // Early-out and rely on normal signature parsing to report this error.
      return;
    case 1:
      // Argument must be the message.
      message = arguments[0];
      break;
    case 2:
      // Assume the meaning is (id, message) if id would be a string.
      // Otherwise the meaning is (message, options).
      if (arguments[0]->IsString()) {
        target_id = arguments[0];
        message = arguments[1];
      } else {
        message = arguments[0];
        options = arguments[1];
      }
      break;
    case 3:
      // The meaning in this case is unambiguous.
      target_id = arguments[0];
      message = arguments[1];
      options = arguments[2];
      break;
    case 4:
      // Too many arguments. Early-out and rely on normal signature parsing to
      // report this error.
      return;
    default:
      NOTREACHED();
  }

  *arguments_out = {target_id, message, options, response_callback};
}

// Handler for the extensionId property on chrome.runtime.
void GetExtensionId(v8::Local<v8::Name> property_name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
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

constexpr char kSendMessageChannel[] = "chrome.runtime.sendMessage";

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

  if (method_name == kSendMessage)
    MassageSendMessageArguments(context->GetIsolate(), arguments);

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
  if (!GetTarget(script_context, arguments[0], &target_id)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error =
        base::StringPrintf(kExtensionIdRequiredErrorTemplate, "sendMessage");
    return result;
  }

  v8::Local<v8::Context> v8_context = script_context->v8_context();
  MessageOptions options;
  if (!arguments[2]->IsNull()) {
    std::string error;
    ParseOptionsResult parse_result = ParseMessageOptions(
        v8_context, arguments[2].As<v8::Object>(), false, &options, &error);
    switch (parse_result) {
      case TYPE_ERROR: {
        RequestResult result(RequestResult::INVALID_INVOCATION);
        result.error = std::move(error);
        return result;
      }
      case THROWN:
        return RequestResult(RequestResult::THROWN);
      case SUCCESS:
        break;
    }
  }

  v8::Local<v8::Value> v8_message = arguments[1];
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(v8_context, v8_message);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = "Illegal argument to runtime.sendMessage for 'message'.";
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[3]->IsNull())
    response_callback = arguments[3].As<v8::Function>();

  messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForExtension(target_id),
      kSendMessageChannel, options.include_tls_channel_id, *message,
      response_callback);

  return RequestResult(RequestResult::HANDLED);
}

RequestResult RuntimeHooksDelegate::HandleSendNativeMessage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(3u, arguments.size());

  std::string application_name = gin::V8ToString(arguments[0]);

  v8::Local<v8::Value> v8_message = arguments[1];
  DCHECK(!v8_message.IsEmpty());
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(script_context->v8_context(), v8_message);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error =
        "Illegal argument to runtime.sendNativeMessage for 'message'.";
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
  if (!GetTarget(script_context, arguments[0], &target_id)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error =
        base::StringPrintf(kExtensionIdRequiredErrorTemplate, "connect");
    return result;
  }

  MessageOptions options;
  if (!arguments[1]->IsNull()) {
    std::string error;
    ParseOptionsResult parse_result = ParseMessageOptions(
        script_context->v8_context(), arguments[1].As<v8::Object>(), true,
        &options, &error);
    switch (parse_result) {
      case TYPE_ERROR: {
        RequestResult result(RequestResult::INVALID_INVOCATION);
        result.error = std::move(error);
        return result;
      }
      case THROWN:
        return RequestResult(RequestResult::THROWN);
      case SUCCESS:
        break;
    }
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
