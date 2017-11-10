// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_util.h"

#include <string>

#include "base/logging.h"
#include "extensions/common/api/messaging/message.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {
namespace messaging_util {

const char kSendMessageChannel[] = "chrome.runtime.sendMessage";
const char kSendRequestChannel[] = "chrome.extension.sendRequest";

const char kOnMessageEvent[] = "runtime.onMessage";
const char kOnMessageExternalEvent[] = "runtime.onMessageExternal";
const char kOnRequestEvent[] = "extension.onRequest";
const char kOnRequestExternalEvent[] = "extension.onRequestExternal";
const char kOnConnectEvent[] = "runtime.onConnect";
const char kOnConnectExternalEvent[] = "runtime.onConnectExternal";

const int kNoFrameId = -1;

std::unique_ptr<Message> MessageFromV8(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  v8::Isolate* isolate = context->GetIsolate();
  v8::Context::Scope context_scope(context);

  // TODO(devlin): For some reason, we don't use the signature for
  // Port.postMessage when evaluating the parameters. We probably should, but
  // we don't know how many extensions that may break. It would be good to
  // investigate, and, ideally, use the signature.

  if (value->IsUndefined()) {
    // JSON.stringify won't serialized undefined (it returns undefined), but it
    // will serialized null. We've always converted undefined to null in JS
    // bindings, so preserve this behavior for now.
    value = v8::Null(isolate);
  }

  bool success = false;
  v8::Local<v8::String> stringified;
  {
    v8::TryCatch try_catch(isolate);
    success = v8::JSON::Stringify(context, value).ToLocal(&stringified);
  }

  std::string message;
  if (success) {
    message = gin::V8ToString(stringified);
    // JSON.stringify can fail to produce a string value in one of two ways: it
    // can throw an exception (as with unserializable objects), or it can return
    // `undefined` (as with e.g. passing a function). If JSON.stringify returns
    // `undefined`, the v8 API then coerces it to the string value "undefined".
    // Check for this, and consider it a failure (since we didn't properly
    // serialize a value).
    success = message != "undefined";
  }

  if (!success)
    return nullptr;

  return std::make_unique<Message>(
      message, blink::WebUserGestureIndicator::IsProcessingUserGesture());
}

v8::Local<v8::Value> MessageToV8(v8::Local<v8::Context> context,
                                 const Message& message) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> v8_message_string =
      gin::StringToV8(isolate, message.data);
  v8::Local<v8::Value> parsed_message;
  v8::TryCatch try_catch(isolate);
  if (!v8::JSON::Parse(context, v8_message_string).ToLocal(&parsed_message)) {
    NOTREACHED();
    return v8::Local<v8::Value>();
  }
  return parsed_message;
}

int ExtractIntegerId(v8::Local<v8::Value> value) {
  // Account for -0, which is a valid integer, but is stored as a number in v8.
  DCHECK(value->IsNumber() &&
         (value->IsInt32() || value.As<v8::Number>()->Value() == 0.0));
  return value->Int32Value();
}

ParseOptionsResult ParseMessageOptions(v8::Local<v8::Context> context,
                                       v8::Local<v8::Object> v8_options,
                                       int flags,
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
  gin::Dictionary options_dict(isolate, v8_options);
  if ((flags & PARSE_CHANNEL_NAME) != 0) {
    v8::Local<v8::Value> v8_channel_name;
    if (!options_dict.Get("name", &v8_channel_name))
      return THROWN;

    if (!v8_channel_name->IsUndefined()) {
      if (!v8_channel_name->IsString()) {
        *error_out = "connectInfo.name must be a string.";
        return TYPE_ERROR;
      }
      options.channel_name = gin::V8ToString(v8_channel_name);
    }
  }

  if ((flags & PARSE_INCLUDE_TLS_CHANNEL_ID) != 0) {
    v8::Local<v8::Value> v8_include_tls_channel_id;
    if (!options_dict.Get("includeTlsChannelId", &v8_include_tls_channel_id))
      return THROWN;

    if (!v8_include_tls_channel_id->IsUndefined()) {
      if (!v8_include_tls_channel_id->IsBoolean()) {
        *error_out = "connectInfo.includeTlsChannelId must be a boolean.";
        return TYPE_ERROR;
      }
      options.include_tls_channel_id =
          v8_include_tls_channel_id->BooleanValue();
    }
  }

  if ((flags & PARSE_FRAME_ID) != 0) {
    v8::Local<v8::Value> v8_frame_id;
    if (!options_dict.Get("frameId", &v8_frame_id))
      return THROWN;

    if (!v8_frame_id->IsUndefined()) {
      if (!v8_frame_id->IsInt32() &&
          (!v8_frame_id->IsNumber() ||
           v8_frame_id.As<v8::Number>()->Value() != 0.0)) {
        *error_out = "connectInfo.frameId must be an integer.";
        return TYPE_ERROR;
      }
      options.frame_id = v8_frame_id->Int32Value();
    }
  }

  *options_out = std::move(options);
  return SUCCESS;
}

}  // namespace messaging_util
}  // namespace extensions
