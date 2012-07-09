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
  RouteFunction("StartRequest",
                base::Bind(&SendRequestNatives::StartRequest,
                           base::Unretained(this)));
}

// Starts an API request to the browser, with an optional callback.  The
// callback will be dispatched to EventBindings::HandleResponse.
v8::Handle<v8::Value> SendRequestNatives::StartRequest(
    const v8::Arguments& args) {
  std::string name = *v8::String::AsciiValue(args[0]);
  // args[1] is the request args object.
  bool has_callback = args[2]->BooleanValue();
  bool for_io_thread = args[3]->BooleanValue();

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  // Make "undefined" the same as "null" for optional arguments, but for objects
  // strip nulls (like {foo: null}, and therefore {foo: undefined} as well) to
  // make it easier for extension APIs to check for optional arguments.
  converter->SetUndefinedAllowed(true);
  converter->SetStripNullFromObjects(true);

  scoped_ptr<Value> value_args(
      converter->FromV8Value(args[1], v8::Context::GetCurrent()));
  CHECK(value_args.get());
  ListValue* api_args = NULL;
  CHECK(value_args->GetAsList(&api_args));

  return v8::Integer::New(request_sender_->StartRequest(
      name, has_callback, for_io_thread, api_args));
}

}  // namespace extensions
