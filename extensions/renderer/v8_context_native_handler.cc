// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/v8_context_native_handler.h"

#include "base/bind.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

V8ContextNativeHandler::V8ContextNativeHandler(ScriptContext* context,
                                               Dispatcher* dispatcher)
    : ObjectBackedNativeHandler(context),
      context_(context),
      dispatcher_(dispatcher) {
  RouteFunction("GetAvailability",
                base::Bind(&V8ContextNativeHandler::GetAvailability,
                           base::Unretained(this)));
  RouteFunction("GetModuleSystem",
                base::Bind(&V8ContextNativeHandler::GetModuleSystem,
                           base::Unretained(this)));
  RouteFunction(
      "RunWithNativesEnabledModuleSystem",
      base::Bind(&V8ContextNativeHandler::RunWithNativesEnabledModuleSystem,
                 base::Unretained(this)));
}

void V8ContextNativeHandler::GetAvailability(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  v8::Isolate* isolate = args.GetIsolate();
  std::string api_name = *v8::String::Utf8Value(args[0]->ToString());
  Feature::Availability availability = context_->GetAvailability(api_name);

  v8::Handle<v8::Object> ret = v8::Object::New(isolate);
  ret->Set(v8::String::NewFromUtf8(isolate, "is_available"),
           v8::Boolean::New(isolate, availability.is_available()));
  ret->Set(v8::String::NewFromUtf8(isolate, "message"),
           v8::String::NewFromUtf8(isolate, availability.message().c_str()));
  ret->Set(v8::String::NewFromUtf8(isolate, "result"),
           v8::Integer::New(isolate, availability.result()));
  args.GetReturnValue().Set(ret);
}

void V8ContextNativeHandler::GetModuleSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
  v8::Handle<v8::Context> v8_context =
      v8::Handle<v8::Object>::Cast(args[0])->CreationContext();
  ScriptContext* context =
      dispatcher_->script_context_set().GetByV8Context(v8_context);
  args.GetReturnValue().Set(context->module_system()->NewInstance());
}

void V8ContextNativeHandler::RunWithNativesEnabledModuleSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  v8::Handle<v8::Value> call_with_args[] = {
    context()->module_system()->NewInstance()
  };
  ModuleSystem::NativesEnabledScope natives_enabled(context()->module_system());
  context()->CallFunction(
      v8::Handle<v8::Function>::Cast(args[0]), 1, call_with_args);
}

}  // namespace extensions
