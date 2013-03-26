// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/bind.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/api_activity_logger.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"

using content::V8ValueConverter;

namespace extensions {

APIActivityLogger::APIActivityLogger(
    Dispatcher* dispatcher, v8::Handle<v8::Context> v8_context)
    : ChromeV8Extension(dispatcher, v8_context) {
  RouteFunction("LogActivity", base::Bind(&APIActivityLogger::LogActivity));
}

// static
v8::Handle<v8::Value> APIActivityLogger::LogActivity(
    const v8::Arguments& args) {
  DCHECK_GT(args.Length(), 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  DCHECK(args[2]->IsArray());

  // Get the simple values.
  std::string ext_id = *v8::String::AsciiValue(args[0]->ToString());
  ExtensionHostMsg_APIAction_Params params;
  params.api_call = *v8::String::AsciiValue(args[1]->ToString());
  if (args.Length() == 4)  // Extras are optional.
    params.extra = *v8::String::AsciiValue(args[3]->ToString());
  else
    params.extra = "";

  // Get the array of api call arguments.
  v8::Local<v8::Array> arg_array = v8::Local<v8::Array>::Cast(args[2]);
  if (arg_array->Length() > 0) {
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    scoped_ptr<ListValue> arg_list(new ListValue());
    for (size_t i = 0; i < arg_array->Length(); ++i) {
      arg_list->Set(i,
                    converter->FromV8Value(arg_array->Get(i),
                    v8::Context::GetCurrent()));
    }
    params.arguments.Swap(arg_list.get());
  }

  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_AddAPIActionToActivityLog(ext_id, params));

  return v8::Undefined();
}

}  // namespace extensions

