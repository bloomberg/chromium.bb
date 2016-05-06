// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/object_backed_native_handler.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "content/public/child/worker_thread.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

// A wrapper around a handler function that will check if the context is allowed
// before passing the arguments to the handler function.
using HandlerFunctionCheck =
    base::Callback<void(const v8::FunctionCallbackInfo<v8::Value>&,
                        const v8::Local<v8::Context>&)>;

// Key for the base::Bound routed function.
const char* kHandlerFunction = "handler_function";

// Checks whether a given |context| has any of the |allowed_features|. This
// treats an empty |allowed_features| as indicating that any context has access.
bool IsContextAllowed(const std::vector<std::string>& allowed_features,
                      const v8::Local<v8::Context>& context) {
  // We can't access the ScriptContextSet on a worker thread. Luckily, we also
  // don't inject many bindings into worker threads.
  // TODO(devlin): Figure out a way around this.
  if (content::WorkerThread::GetCurrentId() == 0)
    return true;
  if (allowed_features.empty())
    return true;
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  // TODO(devlin): Eventually, we should fail if script_context is null.
  if (!script_context)
    return true;
  for (const std::string& feature : allowed_features) {
    if (script_context->GetAvailability(feature).is_available())
      return true;
  }

  return false;
}

// Checks whether the given |context| has access to any of the necessary
// |allowed_features| and calls |function|.
void ValidateAndCall(const ObjectBackedNativeHandler::HandlerFunction& function,
                     const std::vector<std::string>& allowed_features,
                     const v8::FunctionCallbackInfo<v8::Value>& args,
                     const v8::Local<v8::Context>& context) {
  if (!IsContextAllowed(allowed_features, context)) {
    NOTREACHED();
    return;
  }
  function.Run(args);
}

}  // namespace

ObjectBackedNativeHandler::ObjectBackedNativeHandler(ScriptContext* context)
    : router_data_(context->isolate()),
      context_(context),
      object_template_(context->isolate(),
                       v8::ObjectTemplate::New(context->isolate())) {
}

ObjectBackedNativeHandler::~ObjectBackedNativeHandler() {
}

v8::Local<v8::Object> ObjectBackedNativeHandler::NewInstance() {
  return v8::Local<v8::ObjectTemplate>::New(GetIsolate(), object_template_)
      ->NewInstance();
}

// static
void ObjectBackedNativeHandler::Router(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> data = args.Data().As<v8::Object>();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> handler_function_value;
  // See comment in header file for why we do this.
  if (!GetPrivate(context, data, kHandlerFunction, &handler_function_value) ||
      handler_function_value->IsUndefined()) {
    ScriptContext* script_context =
        ScriptContextSet::GetContextByV8Context(context);
    console::Error(script_context ? script_context->GetRenderFrame() : nullptr,
                   "Extension view no longer exists");
    return;
  }

  // This CHECK is *important*. Otherwise, we'll go around happily executing
  // something random.  See crbug.com/548273.
  CHECK(handler_function_value->IsExternal());
  static_cast<HandlerFunctionCheck*>(
      handler_function_value.As<v8::External>()->Value())->Run(args, context);

  // Verify that the return value, if any, is accessible by the context.
  v8::ReturnValue<v8::Value> ret = args.GetReturnValue();
  v8::Local<v8::Value> ret_value = ret.Get();
  if (ret_value->IsObject() && !ret_value->IsNull() &&
      !ContextCanAccessObject(context, v8::Local<v8::Object>::Cast(ret_value),
                              true)) {
    NOTREACHED() << "Insecure return value";
    ret.SetUndefined();
  }
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const HandlerFunction& handler_function) {
  RouteFunction(name, std::vector<std::string>(), handler_function);
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const std::string& feature_name,
    const HandlerFunction& handler_function) {
  RouteFunction(
      name, std::vector<std::string>(1, feature_name), handler_function);
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const std::vector<std::string>& features,
    const HandlerFunction& handler_function) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  HandlerFunctionCheck check =
      base::Bind(&ValidateAndCall, handler_function, features);
  SetPrivate(data, kHandlerFunction,
             v8::External::New(isolate, new HandlerFunctionCheck(check)));
  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(isolate, Router, data);
  v8::Local<v8::ObjectTemplate>::New(isolate, object_template_)
      ->Set(isolate, name.c_str(), function_template);
  router_data_.Append(data);
}

v8::Isolate* ObjectBackedNativeHandler::GetIsolate() const {
  return context_->isolate();
}

void ObjectBackedNativeHandler::Invalidate() {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  for (size_t i = 0; i < router_data_.Size(); i++) {
    v8::Local<v8::Object> data = router_data_.Get(i);
    v8::Local<v8::Value> handler_function_value;
    CHECK(GetPrivate(data, kHandlerFunction, &handler_function_value));
    delete static_cast<HandlerFunction*>(
        handler_function_value.As<v8::External>()->Value());
    DeletePrivate(data, kHandlerFunction);
  }

  router_data_.Clear();
  object_template_.Reset();

  NativeHandler::Invalidate();
}

// static
bool ObjectBackedNativeHandler::ContextCanAccessObject(
    const v8::Local<v8::Context>& context,
    const v8::Local<v8::Object>& object,
    bool allow_null_context) {
  if (object->IsNull())
    return true;
  if (context == object->CreationContext())
    return true;
  ScriptContext* other_script_context =
      ScriptContextSet::GetContextByObject(object);
  if (!other_script_context || !other_script_context->web_frame())
    return allow_null_context;

  return blink::WebFrame::scriptCanAccess(other_script_context->web_frame());
}

void ObjectBackedNativeHandler::SetPrivate(v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value> value) {
  SetPrivate(context_->v8_context(), obj, key, value);
}

// static
void ObjectBackedNativeHandler::SetPrivate(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value> value) {
  obj->SetPrivate(context, v8::Private::ForApi(context->GetIsolate(),
                                               v8::String::NewFromUtf8(
                                                   context->GetIsolate(), key)),
                  value)
      .FromJust();
}

bool ObjectBackedNativeHandler::GetPrivate(v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value>* result) {
  return GetPrivate(context_->v8_context(), obj, key, result);
}

// static
bool ObjectBackedNativeHandler::GetPrivate(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value>* result) {
  return obj->GetPrivate(context,
                         v8::Private::ForApi(context->GetIsolate(),
                                             v8::String::NewFromUtf8(
                                                 context->GetIsolate(), key)))
      .ToLocal(result);
}

void ObjectBackedNativeHandler::DeletePrivate(v8::Local<v8::Object> obj,
                                              const char* key) {
  DeletePrivate(context_->v8_context(), obj, key);
}

// static
void ObjectBackedNativeHandler::DeletePrivate(v8::Local<v8::Context> context,
                                              v8::Local<v8::Object> obj,
                                              const char* key) {
  obj->DeletePrivate(context,
                     v8::Private::ForApi(
                         context->GetIsolate(),
                         v8::String::NewFromUtf8(context->GetIsolate(), key)))
      .FromJust();
}

}  // namespace extensions
