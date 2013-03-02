// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/binding_generating_native_handler.h"

#include "chrome/renderer/extensions/module_system.h"

namespace extensions {

BindingGeneratingNativeHandler::BindingGeneratingNativeHandler(
    ModuleSystem* module_system,
    const std::string& api_name,
    const std::string& bind_to)
    : module_system_(module_system),
      api_name_(api_name),
      bind_to_(bind_to) {
}

v8::Handle<v8::Object> BindingGeneratingNativeHandler::NewInstance() {
  v8::HandleScope scope;
  v8::Handle<v8::Object> binding = v8::Handle<v8::Object>::Cast(
      v8::Handle<v8::Object>::Cast(module_system_->Require(
          "binding"))->Get(v8::String::New("Binding")));
  v8::Handle<v8::Function> create = v8::Handle<v8::Function>::Cast(
      binding->Get(v8::String::New("create")));
  v8::Handle<v8::Value> argv[] = { v8::String::New(api_name_.c_str()) };
  v8::Handle<v8::Object> binding_instance = v8::Handle<v8::Object>::Cast(
      create->Call(binding, 1, argv));
  v8::Handle<v8::Function> generate = v8::Handle<v8::Function>::Cast(
      binding_instance->Get(v8::String::New("generate")));
  v8::Handle<v8::Value> compiled_schema =
      generate->Call(binding_instance, 0, NULL);
  v8::Handle<v8::Object> object = v8::Object::New();
  object->Set(v8::String::New(bind_to_.c_str()), compiled_schema);
  return scope.Close(object);
}

} // extensions
