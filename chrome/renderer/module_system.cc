// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/module_system.h"

#include "base/bind.h"

namespace {

const char* kLazyObjectSource = "lazy_object_source";
const char* kLazyObjectName = "lazy_object_name";
const char* kLazyObject = "lazy_object";

v8::Handle<v8::Object> EvaluateLazySource(v8::Handle<v8::Object> object) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> source_value =
      object->GetHiddenValue(v8::String::New(kLazyObjectSource));
  CHECK(!source_value.IsEmpty());
  v8::Handle<v8::String> source = v8::Handle<v8::String>::Cast(source_value);
  v8::Handle<v8::Value> name =
      object->GetHiddenValue(v8::String::New(kLazyObjectName));
  CHECK(!name.IsEmpty());
  CHECK(name->IsString());
  v8::Handle<v8::Script> script = v8::Script::New(source, name);
  v8::Handle<v8::Value> result = script->Run();
  CHECK(result->IsObject());
  return handle_scope.Close(v8::Handle<v8::Object>::Cast(result));
}

v8::Handle<v8::Value> LazyObjectGetter(
    const v8::Local<v8::String> property,
    const v8::AccessorInfo& info) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> object = info.Holder();
  v8::Handle<v8::String> lazy_object_name = v8::String::New(kLazyObject);
  v8::Handle<v8::Value> lazy_object_value =
      object->GetHiddenValue(lazy_object_name);
  CHECK(lazy_object_value.IsEmpty() || lazy_object_value->IsObject());
  v8::Handle<v8::Object> lazy_object =
      v8::Handle<v8::Object>::Cast(lazy_object_value);
  if (lazy_object.IsEmpty()) {
    lazy_object = EvaluateLazySource(object);
    object->SetHiddenValue(lazy_object_name, lazy_object);
  }
  return handle_scope.Close(lazy_object->Get(property));
}

} // namespace

ModuleSystem::ModuleSystem(SourceMap* source_map)
    : source_map_(source_map),
      natives_enabled_(true) {
  RouteFunction("require",
      base::Bind(&ModuleSystem::RequireForJs, base::Unretained(this)));
  RouteFunction("requireNative",
      base::Bind(&ModuleSystem::GetNative, base::Unretained(this)));

  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  global->SetHiddenValue(v8::String::New("modules"), v8::Object::New());
}

ModuleSystem::~ModuleSystem() {
}

void ModuleSystem::Require(const std::string& module_name) {
  v8::HandleScope handle_scope;
  RequireForJsInner(v8::String::New(module_name.c_str()));
}

v8::Handle<v8::Value> ModuleSystem::RequireForJs(const v8::Arguments& args) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::String> module_name = args[0]->ToString();
  return handle_scope.Close(RequireForJsInner(module_name));
}

v8::Handle<v8::Value> ModuleSystem::RequireForJsInner(
    v8::Handle<v8::String> module_name) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  v8::Handle<v8::Object> modules(v8::Handle<v8::Object>::Cast(
      global->GetHiddenValue(v8::String::New("modules"))));
  v8::Handle<v8::Value> exports(modules->Get(module_name));
  if (!exports->IsUndefined())
    return handle_scope.Close(exports);
  v8::Handle<v8::Value> source(GetSource(module_name));
  if (source->IsUndefined())
    return handle_scope.Close(v8::Undefined());
  v8::Handle<v8::String> wrapped_source(WrapSource(
      v8::Handle<v8::String>::Cast(source)));
  v8::Handle<v8::Function> func =
      v8::Handle<v8::Function>::Cast(RunString(wrapped_source, module_name));
  exports = v8::Object::New();
  v8::Handle<v8::Object> natives(NewInstance());
  v8::Handle<v8::Value> args[] = {
    natives->Get(v8::String::NewSymbol("require")),
    natives->Get(v8::String::NewSymbol("requireNative")),
    exports,
  };
  func->Call(global, 3, args);
  modules->Set(module_name, exports);
  return handle_scope.Close(exports);
}

void ModuleSystem::RegisterNativeHandler(const std::string& name,
    scoped_ptr<NativeHandler> native_handler) {
  native_handler_map_[name] =
      linked_ptr<NativeHandler>(native_handler.release());
}

void ModuleSystem::RunString(const std::string& code, const std::string& name) {
  v8::HandleScope handle_scope;
  RunString(v8::String::New(code.c_str()), v8::String::New(name.c_str()));
}

// static
v8::Handle<v8::Object> ModuleSystem::CreateLazyObject(
    const std::string& source_name, v8::Handle<v8::String> source) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::ObjectTemplate> object_template = v8::ObjectTemplate::New();
  object_template->SetNamedPropertyHandler(LazyObjectGetter, NULL);
  v8::Handle<v8::Object> object = object_template->NewInstance();
  object->SetHiddenValue(v8::String::New(kLazyObjectSource), source);
  object->SetHiddenValue(v8::String::New(kLazyObjectName),
                         v8::String::New(source_name.c_str()));

  return handle_scope.Close(object);
}

v8::Handle<v8::Value> ModuleSystem::RunString(v8::Handle<v8::String> code,
                                              v8::Handle<v8::String> name) {
  v8::HandleScope handle_scope;
  return handle_scope.Close(v8::Script::New(code, name)->Run());
}

v8::Handle<v8::Value> ModuleSystem::GetSource(
    v8::Handle<v8::String> source_name) {
  v8::HandleScope handle_scope;
  std::string module_name = *v8::String::AsciiValue(source_name);
  if (!source_map_->Contains(module_name))
    return v8::Undefined();
  return handle_scope.Close(source_map_->GetSource(module_name));
}

v8::Handle<v8::Value> ModuleSystem::GetNative(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  if (!natives_enabled_)
    return ThrowException("Natives disabled");
  std::string native_name = *v8::String::AsciiValue(args[0]->ToString());
  NativeHandlerMap::iterator i = native_handler_map_.find(native_name);
  if (i == native_handler_map_.end())
    return v8::Undefined();
  return i->second->NewInstance();
}

v8::Handle<v8::String> ModuleSystem::WrapSource(v8::Handle<v8::String> source) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::String> left =
      v8::String::New("(function(require, requireNative, exports) {");
  v8::Handle<v8::String> right = v8::String::New("\n})");
  return handle_scope.Close(
      v8::String::Concat(left, v8::String::Concat(source, right)));
}

v8::Handle<v8::Value> ModuleSystem::ThrowException(const std::string& message) {
  return v8::ThrowException(v8::String::New(message.c_str()));
}
