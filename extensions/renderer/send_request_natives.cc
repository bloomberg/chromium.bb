// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/send_request_natives.h"

#include "base/json/json_reader.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/script_context.h"

using content::V8ValueConverter;

namespace extensions {

SendRequestNatives::SendRequestNatives(RequestSender* request_sender,
                                       ScriptContext* context)
    : ObjectBackedNativeHandler(context), request_sender_(request_sender) {
  RouteFunction("GetNextRequestId",
                base::Bind(&SendRequestNatives::GetNextRequestId,
                           base::Unretained(this)));
  RouteFunction(
      "StartRequest",
      base::Bind(&SendRequestNatives::StartRequest, base::Unretained(this)));
  RouteFunction(
      "GetGlobal",
      base::Bind(&SendRequestNatives::GetGlobal, base::Unretained(this)));
}

void SendRequestNatives::GetNextRequestId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      static_cast<int32_t>(request_sender_->GetNextRequestId()));
}

// Starts an API request to the browser, with an optional callback.  The
// callback will be dispatched to EventBindings::HandleResponse.
void SendRequestNatives::StartRequest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(6, args.Length());
  std::string name = *v8::String::Utf8Value(args[0]);
  int request_id = args[2]->Int32Value();
  bool has_callback = args[3]->BooleanValue();
  bool for_io_thread = args[4]->BooleanValue();
  bool preserve_null_in_objects = args[5]->BooleanValue();

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  // See http://crbug.com/149880. The context menus APIs relies on this, but
  // we shouldn't really be doing it (e.g. for the sake of the storage API).
  converter->SetFunctionAllowed(true);

  if (!preserve_null_in_objects)
    converter->SetStripNullFromObjects(true);

  scoped_ptr<base::Value> value_args(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!value_args.get() || !value_args->IsType(base::Value::TYPE_LIST)) {
    NOTREACHED() << "Unable to convert args passed to StartRequest";
    return;
  }

  request_sender_->StartRequest(
      context(),
      name,
      request_id,
      has_callback,
      for_io_thread,
      static_cast<base::ListValue*>(value_args.get()));
}

void SendRequestNatives::GetGlobal(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsObject());
  args.GetReturnValue().Set(
      v8::Handle<v8::Object>::Cast(args[0])->CreationContext()->Global());
}

}  // namespace extensions
