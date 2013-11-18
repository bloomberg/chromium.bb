// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_registry.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"

using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::StackTrace;
using v8::String;
using v8::Value;

namespace gin {

struct PendingModule {
  PendingModule();
  ~PendingModule();

  std::string id;
  std::vector<std::string> dependencies;
  Persistent<Value> factory;
};

PendingModule::PendingModule() {
}

PendingModule::~PendingModule() {
  factory.Reset();
}

namespace {

void Define(const v8::FunctionCallbackInfo<Value>& info) {
  Arguments args(info);

  if (!info.Length())
    return args.ThrowTypeError("At least one argument is required.");

  std::string id;
  std::vector<std::string> dependencies;
  Handle<Value> factory;

  if (args.PeekNext()->IsString())
    args.GetNext(&id);
  if (args.PeekNext()->IsArray())
    args.GetNext(&dependencies);
  if (!args.GetNext(&factory))
    return args.ThrowError();

  scoped_ptr<PendingModule> pending(new PendingModule);
  pending->id = id;
  pending->dependencies = dependencies;
  pending->factory.Reset(args.isolate(), factory);

  ModuleRegistry* registry =
      ModuleRegistry::From(args.isolate()->GetCurrentContext());
  registry->AddPendingModule(args.isolate(), pending.Pass());
}

WrapperInfo g_wrapper_info = {};

Local<FunctionTemplate> GetDefineTemplate(Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  Local<FunctionTemplate> templ = data->GetFunctionTemplate(
      &g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = FunctionTemplate::New(Define);
    data->SetFunctionTemplate(&g_wrapper_info, templ);
  }
  return templ;
}

Handle<String> GetHiddenValueKey(Isolate* isolate) {
  return StringToSymbol(isolate, "::gin::ModuleRegistry");
}

std::string GetImplicitModuleName(const std::string& explicit_name) {
  if (!explicit_name.empty())
    return explicit_name;
  std::string implicit_name;
  Handle<StackTrace> trace = StackTrace::CurrentStackTrace(1);
  Handle<String> script_name = trace->GetFrame(0)->GetScriptName();
  if (!script_name.IsEmpty())
    ConvertFromV8(script_name, &implicit_name);
  return implicit_name;
}

}  // namespace

ModuleRegistry::ModuleRegistry(Isolate* isolate)
    : modules_(isolate, Object::New()) {
}

ModuleRegistry::~ModuleRegistry() {
  modules_.Reset();
}

void ModuleRegistry::RegisterGlobals(Isolate* isolate,
                                     Handle<ObjectTemplate> templ) {
  templ->Set(StringToSymbol(isolate, "define"), GetDefineTemplate(isolate));
}

ModuleRegistry* ModuleRegistry::From(Handle<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Handle<String> key = GetHiddenValueKey(isolate);
  Handle<Value> value = context->Global()->GetHiddenValue(key);
  Handle<External> external;
  if (value.IsEmpty() || !ConvertFromV8(value, &external)) {
    PerContextData* data = PerContextData::From(context);
    if (!data)
      return NULL;
    ModuleRegistry* registry = new ModuleRegistry(isolate);
    context->Global()->SetHiddenValue(key, External::New(isolate, registry));
    data->AddSupplement(scoped_ptr<ContextSupplement>(registry));
    return registry;
  }
  return static_cast<ModuleRegistry*>(external->Value());
}

void ModuleRegistry::AddBuiltinModule(Isolate* isolate,
                                      const std::string& id,
                                      Handle<ObjectTemplate> templ) {
  DCHECK(!id.empty());
  RegisterModule(isolate, id, templ->NewInstance());
}

void ModuleRegistry::AddPendingModule(Isolate* isolate,
                                      scoped_ptr<PendingModule> pending) {
  AttemptToLoad(isolate, pending.Pass());
}

void ModuleRegistry::RegisterModule(Isolate* isolate,
                                    const std::string& id,
                                    Handle<Value> module) {
  if (id.empty() || module.IsEmpty())
    return;

  unsatisfied_dependencies_.erase(id);
  available_modules_.insert(id);
  Handle<Object> modules = Local<Object>::New(isolate, modules_);
  modules->Set(StringToSymbol(isolate, id), module);
}

void ModuleRegistry::Detach(Handle<Context> context) {
  context->Global()->SetHiddenValue(GetHiddenValueKey(context->GetIsolate()),
                                    Handle<Value>());
}

bool ModuleRegistry::CheckDependencies(PendingModule* pending) {
  size_t num_missing_dependencies = 0;
  size_t len = pending->dependencies.size();
  for (size_t i = 0; i < len; ++i) {
    const std::string& dependency = pending->dependencies[i];
    if (available_modules_.count(dependency))
      continue;
    unsatisfied_dependencies_.insert(dependency);
    num_missing_dependencies++;
  }
  return num_missing_dependencies == 0;
}

void ModuleRegistry::Load(Isolate* isolate, scoped_ptr<PendingModule> pending) {
  if (!pending->id.empty() && available_modules_.count(pending->id))
    return;  // We've already loaded this module.

  Handle<Object> modules = Local<Object>::New(isolate, modules_);
  size_t argc = pending->dependencies.size();
  std::vector<Handle<Value> > argv(argc);
  for (size_t i = 0; i < argc; ++i) {
    Handle<String> key = StringToSymbol(isolate, pending->dependencies[i]);
    DCHECK(modules->HasOwnProperty(key));
    argv[i] = modules->Get(key);
  }

  Handle<Value> module = Local<Value>::New(isolate, pending->factory);

  Handle<Function> factory;
  if (ConvertFromV8(module, &factory)) {
    Handle<Object> global = isolate->GetCurrentContext()->Global();
    module = factory->Call(global, argc, argv.data());
    // TODO(abarth): What should we do with exceptions?
  }

  RegisterModule(isolate, GetImplicitModuleName(pending->id), module);
}

bool ModuleRegistry::AttemptToLoad(Isolate* isolate,
                                   scoped_ptr<PendingModule> pending) {
  if (!CheckDependencies(pending.get())) {
    pending_modules_.push_back(pending.release());
    return false;
  }
  Load(isolate, pending.Pass());
  return true;
}

void ModuleRegistry::AttemptToLoadMoreModules(Isolate* isolate) {
  bool keep_trying = true;
  while (keep_trying) {
    keep_trying = false;
    PendingModuleVector pending_modules;
    pending_modules.swap(pending_modules_);
    for (size_t i = 0; i < pending_modules.size(); ++i) {
      scoped_ptr<PendingModule> pending(pending_modules[i]);
      pending_modules[i] = NULL;
      if (AttemptToLoad(isolate, pending.Pass()))
        keep_trying = true;
    }
  }
}

}  // namespace gin
