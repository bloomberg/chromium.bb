// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/send_request_natives.h"

#include "base/json/json_reader.h"
#include "content/public/renderer/v8_value_converter.h"
#include "chrome/renderer/extensions/extension_request_sender.h"

using content::V8ValueConverter;

namespace extensions {

SendRequestNatives::SendRequestNatives(
    ExtensionDispatcher* extension_dispatcher,
    ExtensionRequestSender* request_sender)
    : ChromeV8Extension(extension_dispatcher),
      request_sender_(request_sender) {
  RouteFunction("GetNextRequestId",
                base::Bind(&SendRequestNatives::GetNextRequestId,
                           base::Unretained(this)));
  RouteFunction("StartRequest",
                base::Bind(&SendRequestNatives::StartRequest,
                           base::Unretained(this)));
}

v8::Handle<v8::Value> SendRequestNatives::GetNextRequestId(
    const v8::Arguments& args) {
  static int next_request_id = 0;
  return v8::Integer::New(next_request_id++);
}

// Starts an API request to the browser, with an optional callback.  The
// callback will be dispatched to EventBindings::HandleResponse.
v8::Handle<v8::Value> SendRequestNatives::StartRequest(
    const v8::Arguments& args) {
  std::string name = *v8::String::AsciiValue(args[0]);
  int request_id = args[2]->Int32Value();
  bool has_callback = args[3]->BooleanValue();
  bool for_io_thread = args[4]->BooleanValue();

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<Value> value_args(
      converter->FromV8Value(args[1], v8::Context::GetCurrent()));
  if (!value_args.get() || !value_args->IsType(Value::TYPE_LIST)) {
    NOTREACHED() << "Unable to convert args passed to StartRequest";
    return v8::Undefined();
  }

  request_sender_->StartRequest(name, request_id, has_callback, for_io_thread,
                                static_cast<ListValue*>(value_args.get()));
  return v8::Undefined();
}

}  // namespace extensions
