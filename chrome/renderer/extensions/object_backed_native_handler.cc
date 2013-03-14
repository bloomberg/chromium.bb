// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/object_backed_native_handler.h"

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "chrome/renderer/extensions/module_system.h"
#include "v8/include/v8.h"

namespace extensions {

// Data to pass to ObjectBackedNativeHandler::Router.
struct ObjectBackedNativeHandler::RouterData {
  RouterData(ObjectBackedNativeHandler* self, HandlerFunction function)
    : self(self), function(function) {}

  ~RouterData() {}

  // The owner of the routed data.
  ObjectBackedNativeHandler* const self;

  // The function to route calls to.
  HandlerFunction function;
};

ObjectBackedNativeHandler::ObjectBackedNativeHandler(
    v8::Handle<v8::Context> context)
    : v8_context_(context),
      object_template_(v8::ObjectTemplate::New()) {
}

ObjectBackedNativeHandler::~ObjectBackedNativeHandler() {
  Invalidate();
}

v8::Handle<v8::Object> ObjectBackedNativeHandler::NewInstance() {
  return object_template_->NewInstance();
}

// static
v8::Handle<v8::Value> ObjectBackedNativeHandler::Router(
    const v8::Arguments& args) {
  RouterData* router_data = static_cast<RouterData*>(
      args.Data().As<v8::External>()->Value());
  // Router can be called during context destruction. Stop.
  if (!router_data->self->is_valid())
    return v8::Handle<v8::Value>();
  return router_data->function.Run(args);
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const HandlerFunction& handler_function) {
  linked_ptr<RouterData> data(new RouterData(this, handler_function));
  // TODO(koz): Investigate using v8's MakeWeak() function instead of holding
  // on to these pointers here.
  router_data_.push_back(data);
  v8::Handle<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(Router, v8::External::New(data.get()));
  object_template_->Set(name.c_str(), function_template);
}

void ObjectBackedNativeHandler::RouteStaticFunction(
    const std::string& name,
    const HandlerFunc handler_func) {
  v8::Handle<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(handler_func, v8::External::New(this));
  object_template_->Set(name.c_str(), function_template);
}

void ObjectBackedNativeHandler::Invalidate() {
  if (!is_valid())
    return;
  object_template_.reset();
  v8_context_.reset();
  NativeHandler::Invalidate();
}

}   // extensions
