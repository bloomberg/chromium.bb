// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/module_system.h"

#include "base/bind.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScopedMicrotaskSuppression.h"

namespace {

const char* kModuleSystem = "module_system";
const char* kModuleName = "module_name";
const char* kModuleField = "module_field";
const char* kModulesField = "modules";

} // namespace

namespace extensions {

ModuleSystem::ModuleSystem(v8::Handle<v8::Context> context,
                           SourceMap* source_map)
    : context_(v8::Persistent<v8::Context>::New(context)),
      source_map_(source_map),
      natives_enabled_(0) {
  RouteFunction("require",
      base::Bind(&ModuleSystem::RequireForJs, base::Unretained(this)));
  RouteFunction("requireNative",
      base::Bind(&ModuleSystem::GetNative, base::Unretained(this)));

  v8::Handle<v8::Object> global(context_->Global());
  global->SetHiddenValue(v8::String::New(kModulesField), v8::Object::New());
  global->SetHiddenValue(v8::String::New(kModuleSystem),
                         v8::External::New(this));
}

ModuleSystem::~ModuleSystem() {
  v8::HandleScope handle_scope;
  // Deleting this value here prevents future lazy field accesses from
  // referencing ModuleSystem after it has been freed.
  context_->Global()->DeleteHiddenValue(v8::String::New(kModuleSystem));
  context_.Dispose();
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

// static
bool ModuleSystem::IsPresentInCurrentContext() {
  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  if (global.IsEmpty())
    return false;
  v8::Handle<v8::Value> module_system =
      global->GetHiddenValue(v8::String::New(kModuleSystem));
  return !module_system.IsEmpty() && !module_system->IsUndefined();
}

void ModuleSystem::HandleException(const v8::TryCatch& try_catch) {
  DumpException(try_catch);
  if (exception_handler_.get())
    exception_handler_->HandleUncaughtException();
}

// static
void ModuleSystem::DumpException(const v8::TryCatch& try_catch) {
  v8::HandleScope handle_scope;

  v8::Handle<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    LOG(ERROR) << "try_catch has no message";
    return;
  }

  std::string resource_name = "<unknown resource>";
  if (!message->GetScriptResourceName().IsEmpty()) {
    resource_name =
        *v8::String::Utf8Value(message->GetScriptResourceName()->ToString());
  }

  std::string error_message = "<no error message>";
  if (!message->Get().IsEmpty())
    error_message = *v8::String::Utf8Value(message->Get());

  std::string stack_trace = "<stack trace unavailable>";
  if (!try_catch.StackTrace().IsEmpty()) {
    v8::String::Utf8Value stack_value(try_catch.StackTrace());
    if (*stack_value)
      stack_trace.assign(*stack_value, stack_value.length());
    else
      stack_trace = "<could not convert stack trace to string>";
  }

  LOG(ERROR) << "[" << resource_name << "(" << message->GetLineNumber() << ")] "
             << error_message
             << "{" << stack_trace << "}";
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
      global->GetHiddenValue(v8::String::New(kModulesField))));
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
  if (func.IsEmpty()) {
    return ThrowException(std::string(*v8::String::AsciiValue(module_name)) +
        ": Bad source");
  }

  exports = v8::Object::New();
  v8::Handle<v8::Object> natives(NewInstance());
  v8::Handle<v8::Value> args[] = {
    natives->Get(v8::String::NewSymbol("require")),
    natives->Get(v8::String::NewSymbol("requireNative")),
    exports,
  };
  {
    WebKit::WebScopedMicrotaskSuppression suppression;
    v8::TryCatch try_catch;
    try_catch.SetCaptureMessage(true);
    func->Call(global, 3, args);
    if (try_catch.HasCaught()) {
      HandleException(try_catch);
      return v8::Undefined();
    }
  }
  modules->Set(module_name, exports);
  return handle_scope.Close(exports);
}

void ModuleSystem::CallModuleMethod(const std::string& module_name,
                                    const std::string& method_name) {
  v8::HandleScope handle_scope;
  v8::Local<v8::Value> module =
      v8::Local<v8::Value>::New(
          RequireForJsInner(v8::String::New(module_name.c_str())));
  if (module.IsEmpty() || !module->IsObject())
    return;
  v8::Local<v8::Value> value =
      v8::Handle<v8::Object>::Cast(module)->Get(
          v8::String::New(method_name.c_str()));
  if (value.IsEmpty() || !value->IsFunction())
    return;
  v8::Handle<v8::Function> func =
      v8::Handle<v8::Function>::Cast(value);
  // TODO(jeremya/koz): refer to context_ here, not the current context.
  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  {
    WebKit::WebScopedMicrotaskSuppression suppression;
    v8::TryCatch try_catch;
    try_catch.SetCaptureMessage(true);
    func->Call(global, 0, NULL);
    if (try_catch.HasCaught())
      HandleException(try_catch);
  }
}

void ModuleSystem::RegisterNativeHandler(const std::string& name,
    scoped_ptr<NativeHandler> native_handler) {
  native_handler_map_[name] =
      linked_ptr<NativeHandler>(native_handler.release());
}

void ModuleSystem::OverrideNativeHandler(const std::string& name) {
  overridden_native_handlers_.insert(name);
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
  v8::Handle<v8::Object> global(v8::Context::GetCurrent()->Global());
  v8::Handle<v8::Value> module_system_value =
      global->GetHiddenValue(v8::String::New(kModuleSystem));
  if (module_system_value->IsUndefined()) {
    // ModuleSystem has been deleted.
    return v8::Undefined();
  }
  ModuleSystem* module_system = static_cast<ModuleSystem*>(
      v8::Handle<v8::External>::Cast(module_system_value)->Value());

  v8::Handle<v8::Object> module;
  {
    NativesEnabledScope scope(module_system);
    module = v8::Handle<v8::Object>::Cast(module_system->RequireForJsInner(
        parameters->Get(v8::String::New(kModuleName))->ToString()));
  }
  if (module.IsEmpty())
    return handle_scope.Close(v8::Handle<v8::Value>());

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

  object->SetAccessor(v8::String::New(field.c_str()),
                      &ModuleSystem::LazyFieldGetter,
                      NULL,
                      parameters);
}

v8::Handle<v8::Value> ModuleSystem::RunString(v8::Handle<v8::String> code,
                                              v8::Handle<v8::String> name) {
  v8::HandleScope handle_scope;
  WebKit::WebScopedMicrotaskSuppression suppression;
  v8::Handle<v8::Value> result;
  v8::TryCatch try_catch;
  try_catch.SetCaptureMessage(true);
  v8::Handle<v8::Script> script(v8::Script::New(code, name));
  if (try_catch.HasCaught()) {
    HandleException(try_catch);
    return handle_scope.Close(result);
  }

  result = script->Run();
  if (try_catch.HasCaught())
    HandleException(try_catch);

  return handle_scope.Close(result);
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
  if (overridden_native_handlers_.count(native_name) > 0u)
    return RequireForJs(args);
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

}  // extensions
