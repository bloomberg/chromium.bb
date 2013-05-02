// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/object_backed_native_handler.h"

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "chrome/renderer/extensions/console.h"
#include "chrome/renderer/extensions/module_system.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {
// Key for the base::Bound routed function.
const char* kHandlerFunction = "handler_function";
}  // namespace

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
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> data = args.Data().As<v8::Object>();

  v8::Handle<v8::Value> handler_function_value =
      data->Get(v8::String::New(kHandlerFunction));
  // See comment in header file for why we do this.
  if (handler_function_value.IsEmpty() ||
      handler_function_value->IsUndefined()) {
    console::Error(v8::Context::GetCalling(),
                   "Extension view no longer exists");
    return v8::Undefined();
  }
  DCHECK(handler_function_value->IsExternal());
  return handle_scope.Close(static_cast<HandlerFunction*>(
      handler_function_value.As<v8::External>()->Value())->Run(args));
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const HandlerFunction& handler_function) {
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(v8_context_.get());

  v8::Persistent<v8::Object> data(v8_context_->GetIsolate(), v8::Object::New());
  data->Set(v8::String::New(kHandlerFunction),
            v8::External::New(new HandlerFunction(handler_function)));
  router_data_.push_back(data);
  v8::Handle<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(Router, data);
  object_template_->Set(name.c_str(), function_template);
}

void ObjectBackedNativeHandler::Invalidate() {
  if (!is_valid())
    return;
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(v8_context_.get());

  for (RouterData::iterator it = router_data_.begin();
       it != router_data_.end(); ++it) {
    v8::Persistent<v8::Object> data = *it;
    v8::Handle<v8::Value> handler_function_value =
        data->Get(v8::String::New(kHandlerFunction));
    CHECK(!handler_function_value.IsEmpty());
    delete static_cast<HandlerFunction*>(
        handler_function_value.As<v8::External>()->Value());
    data->Delete(v8::String::New(kHandlerFunction));
    data.Dispose(v8_context_->GetIsolate());
  }
  object_template_.reset();
  v8_context_.reset();
  NativeHandler::Invalidate();
}

}   // extensions
