// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/native_handler.h"

#include "base/memory/linked_ptr.h"
#include "base/logging.h"
#include "chrome/renderer/extensions/module_system.h"
#include "v8/include/v8.h"

namespace extensions {

NativeHandler::NativeHandler()
    : object_template_(
        v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New())) {
}

NativeHandler::~NativeHandler() {
  object_template_.Dispose();
}

v8::Handle<v8::Object> NativeHandler::NewInstance() {
  return object_template_->NewInstance();
}

// static
v8::Handle<v8::Value> NativeHandler::Router(const v8::Arguments& args) {
  // It is possible for JS code to execute after ModuleSystem has been deleted
  // in which case the native handlers will also have been deleted, making
  // HandlerFunction below point to freed memory.
  if (!ModuleSystem::IsPresentInCurrentContext()) {
    return v8::ThrowException(v8::Exception::Error(
        v8::String::New("ModuleSystem has been deleted")));
  }
  HandlerFunction* handler_function = static_cast<HandlerFunction*>(
      args.Data().As<v8::External>()->Value());
  return handler_function->Run(args);
}

// static
void NativeHandler::DisposeFunction(v8::Persistent<v8::Value> object,
                                    void* parameter) {
  HandlerFunction* handler_function =
      reinterpret_cast<HandlerFunction*>(parameter);

  object.Dispose();
  delete handler_function;
}

void NativeHandler::RouteFunction(const std::string& name,
    const HandlerFunction& handler_function) {
  HandlerFunction* function = new HandlerFunction(handler_function);
  // Deleted in DisposeFunction once v8 garbage collects function_template.
  v8::Persistent<v8::External> function_value =
      v8::Persistent<v8::External>::New(v8::External::New(function));
  function_value.MakeWeak(function, DisposeFunction);
  v8::Handle<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(Router, function_value);
  object_template_->Set(name.c_str(), function_template);
}

void NativeHandler::RouteStaticFunction(const std::string& name,
                                        const HandlerFunc handler_func) {
  v8::Handle<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(handler_func, v8::External::New(this));
  object_template_->Set(name.c_str(), function_template);
}

}   // extensions
