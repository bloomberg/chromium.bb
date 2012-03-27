// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/module_system.h"

#include "base/bind.h"

namespace {

const char* kModuleSystem = "module_system";
const char* kModuleName = "module_name";
const char* kModuleField = "module_field";

} // namespace

ModuleSystem::ModuleSystem(SourceMap* source_map)
    : source_map_(source_map),
      natives_enabled_(0) {
  RouteFunction("require",
      base::Bind(&ModuleSystem::RequireForJs, base::Unretained(this)));
  RouteFunction("requireNative",
      base::Bind(&ModuleSystem::GetNative, base::Unretained(this)));

  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  global->SetHiddenValue(v8::String::New("modules"), v8::Object::New());
}

ModuleSystem::~ModuleSystem() {
}

ModuleSystem::NativesEnabledScope::NativesEnabledScope(
    ModuleSystem* module_system)
    : module_system_(module_system) {
  module_system_->natives_enabled_++;
}

ModuleSystem::NativesEnabledScope::~NativesEnabledScope() {
  module_system_->natives_enabled_--;
  CHECK_GE(module_system_->natives_enabled_, 0);
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
v8::Handle<v8::Value> ModuleSystem::LazyFieldGetter(
    v8::Local<v8::String> property, const v8::AccessorInfo& info) {
  CHECK(!info.Data().IsEmpty());
  CHECK(info.Data()->IsObject());
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> parameters = v8::Handle<v8::Object>::Cast(info.Data());
  v8::Handle<v8::Value> module_system_value =
      parameters->Get(v8::String::New(kModuleSystem));
  ModuleSystem* module_system = static_cast<ModuleSystem*>(
      v8::Handle<v8::External>::Cast(module_system_value)->Value());

  v8::Handle<v8::Object> module;
  {
    NativesEnabledScope scope(module_system);
    module = module_system->RequireForJsInner(
        parameters->Get(v8::String::New(kModuleName))->ToString())->ToObject();
  }

  v8::Handle<v8::String> field =
      parameters->Get(v8::String::New(kModuleField))->ToString();

  return handle_scope.Close(module->Get(field));
}

void ModuleSystem::SetLazyField(v8::Handle<v8::Object> object,
                                const std::string& field,
                                const std::string& module_name,
                                const std::string& module_field) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> parameters = v8::Object::New();
  parameters->Set(v8::String::New(kModuleName),
                  v8::String::New(module_name.c_str()));
  parameters->Set(v8::String::New(kModuleField),
                  v8::String::New(module_field.c_str()));
  parameters->Set(v8::String::New(kModuleSystem), v8::External::New(this));

  object->SetAccessor(v8::String::New(field.c_str()),
                      &ModuleSystem::LazyFieldGetter,
                      NULL,
                      parameters);
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
  if (natives_enabled_ == 0)
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
