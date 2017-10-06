// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_util.h"

#include <string>

#include "base/logging.h"
#include "extensions/common/api/messaging/message.h"
#include "gin/converter.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {
namespace messaging_util {

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

}  // namespace messaging_util
}  // namespace extensions
