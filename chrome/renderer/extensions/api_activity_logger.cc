// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/bind.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/activity_log_converter_strategy.h"
#include "chrome/renderer/extensions/api_activity_logger.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"

using content::V8ValueConverter;

namespace extensions {

APIActivityLogger::APIActivityLogger(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("LogEvent", base::Bind(&APIActivityLogger::LogEvent));
  RouteFunction("LogAPICall", base::Bind(&APIActivityLogger::LogAPICall));
  RouteFunction("LogBlockedCall",
                base::Bind(&APIActivityLogger::LogBlockedCallWrapper));
}

// static
void APIActivityLogger::LogAPICall(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  LogInternal(APICALL, args);
}

// static
void APIActivityLogger::LogEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  LogInternal(EVENT, args);
}

// static
void APIActivityLogger::LogInternal(
    const CallType call_type,
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GT(args.Length(), 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  DCHECK(args[2]->IsArray());

  std::string ext_id = *v8::String::AsciiValue(args[0]);
  ExtensionHostMsg_APIActionOrEvent_Params params;
  params.api_call = *v8::String::AsciiValue(args[1]);
  if (args.Length() == 4)  // Extras are optional.
    params.extra = *v8::String::AsciiValue(args[3]);
  else
    params.extra = "";

  // Get the array of api call arguments.
  v8::Local<v8::Array> arg_array = v8::Local<v8::Array>::Cast(args[2]);
  if (arg_array->Length() > 0) {
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    ActivityLogConverterStrategy strategy;
    converter->SetFunctionAllowed(true);
    converter->SetStrategy(&strategy);
    scoped_ptr<ListValue> arg_list(new ListValue());
    for (size_t i = 0; i < arg_array->Length(); ++i) {
      arg_list->Set(i,
                    converter->FromV8Value(arg_array->Get(i),
                    v8::Context::GetCurrent()));
    }
    params.arguments.Swap(arg_list.get());
  }

  if (call_type == APICALL) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddAPIActionToActivityLog(ext_id, params));
  } else if (call_type == EVENT) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddEventToActivityLog(ext_id, params));
  }
}

// static
void APIActivityLogger::LogBlockedCallWrapper(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 3);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  DCHECK(args[2]->IsNumber());
  int result;
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  converter->FromV8Value(args[2],
                         v8::Context::GetCurrent())->GetAsInteger(&result);
  LogBlockedCall(*v8::String::AsciiValue(args[0]),
                 *v8::String::AsciiValue(args[1]),
                 static_cast<Feature::AvailabilityResult>(result));
}

// static
void APIActivityLogger::LogBlockedCall(const std::string& extension_id,
                                       const std::string& function_name,
                                       Feature::AvailabilityResult result) {
  // We don't really want to bother logging if it isn't permission related.
  if (result == Feature::INVALID_MIN_MANIFEST_VERSION ||
      result == Feature::INVALID_MAX_MANIFEST_VERSION ||
      result == Feature::UNSUPPORTED_CHANNEL)
    return;
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_AddBlockedCallToActivityLog(extension_id,
                                                       function_name));
}


}  // namespace extensions
