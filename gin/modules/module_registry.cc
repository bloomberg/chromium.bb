// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_registry.h"

#include <assert.h>
#include <string>
#include <vector>
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"

using v8::External;
using v8::Handle;
using v8::Isolate;
using v8::ObjectTemplate;

namespace gin {

struct PendingModule {
  PendingModule();
  ~PendingModule();

  std::string id;
  std::vector<std::string> dependencies;
  v8::Persistent<v8::Value> factory;
};

namespace {

void Define(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  if (!info.Length())
    return args.ThrowTypeError("At least one argument is required.");

  std::string id;
  std::vector<std::string> dependencies;
  Handle<v8::Value> factory;

  if (args.PeekNext()->IsString())
    args.GetNext(&id);
  if (args.PeekNext()->IsArray())
    args.GetNext(&dependencies);
  if (!args.GetNext(&factory))
    return args.ThrowError();

  PendingModule* pending = new PendingModule;
  pending->id = id;
  pending->dependencies = dependencies;
  pending->factory.Reset(args.isolate(), factory);

  ModuleRegistry* registry =
      ModuleRegistry::From(args.isolate()->GetCurrentContext());
  registry->AddPendingModule(args.isolate(), pending);
}

WrapperInfo g_wrapper_info = {};

v8::Local<v8::FunctionTemplate> GetDefineTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> templ = data->GetFunctionTemplate(
      &g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = v8::FunctionTemplate::New(Define);
    data->SetFunctionTemplate(&g_wrapper_info, templ);
  }
  return templ;
}

Handle<v8::String> GetHiddenValueKey(v8::Isolate* isolate) {
  return StringToSymbol(isolate, "::gin::ModuleRegistry");
}

}  // namespace


PendingModule::PendingModule() {
}

PendingModule::~PendingModule() {
  factory.Reset();
}

ModuleRegistry::ModuleRegistry(v8::Isolate* isolate)
    : modules_(isolate, v8::Object::New()) {
}

ModuleRegistry::~ModuleRegistry() {
  for (PendingModuleList::iterator it = pending_modules_.begin();
       it != pending_modules_.end(); ++it) {
    delete *it;
  }
  modules_.Reset();
}

void ModuleRegistry::RegisterGlobals(v8::Isolate* isolate,
                                     Handle<v8::ObjectTemplate> templ) {
  templ->Set(StringToSymbol(isolate, "define"), GetDefineTemplate(isolate));
}

void ModuleRegistry::AddBuiltinModule(Isolate* isolate,
                                      const std::string& id,
                                      Handle<ObjectTemplate> templ) {
  assert(!id.empty());
  Handle<v8::Object> modules = v8::Local<v8::Object>::New(isolate, modules_);
  modules->Set(StringToV8(isolate, id), templ->NewInstance());
}

ModuleRegistry* ModuleRegistry::From(Handle<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  Handle<v8::String> key = GetHiddenValueKey(isolate);
  Handle<v8::Value> value = context->Global()->GetHiddenValue(key);
  Handle<v8::External> external;
  if (value.IsEmpty() || !ConvertFromV8(value, &external)) {
    PerContextData* data = PerContextData::From(context);
    if (!data)
      return NULL;
    ModuleRegistry* registry = new ModuleRegistry(isolate);
    context->Global()->SetHiddenValue(key, v8::External::New(registry));
    data->AddSupplement(registry);
    return registry;
  }
  return static_cast<ModuleRegistry*>(external->Value());
}

void ModuleRegistry::AddPendingModule(v8::Isolate* isolate,
                                      PendingModule* pending) {
  if (AttemptToLoad(isolate, pending))
    AttemptToLoadPendingModules(isolate);
}

void ModuleRegistry::Detach(Handle<v8::Context> context) {
  context->Global()->SetHiddenValue(GetHiddenValueKey(context->GetIsolate()),
                                    Handle<v8::Value>());
}

bool ModuleRegistry::AttemptToLoad(v8::Isolate* isolate,
                                   PendingModule* pending) {
  Handle<v8::Object> modules = v8::Local<v8::Object>::New(isolate, modules_);
  Handle<v8::String> key = StringToV8(isolate, pending->id);

  if (!pending->id.empty() && modules->HasOwnProperty(key)) {
    // We've already loaded a module with this name. Ignore the new one.
    delete pending;
    return true;
  }

  size_t argc = pending->dependencies.size();
  std::vector<Handle<v8::Value> > argv(argc);
  for (size_t i = 0; i < argc; ++i) {
    Handle<v8::String> key = StringToV8(isolate, pending->dependencies[i]);
    if (!modules->HasOwnProperty(key)) {
      pending_modules_.push_back(pending);
      return false;
    }
    argv[i] = modules->Get(key);
  }

  Handle<v8::Value> module = v8::Local<v8::Value>::New(
      isolate, pending->factory);

  Handle<v8::Function> factory;
  if (ConvertFromV8(module, &factory)) {
    v8::Handle<v8::Object> global = isolate->GetCurrentContext()->Global();
    module = factory->Call(global, argc, argv.data());
    // TODO(abarth): What should we do with exceptions?
  }

  if (!pending->id.empty() && !module.IsEmpty())
    modules->Set(key, module);

  delete pending;
  return true;
}

void ModuleRegistry::AttemptToLoadPendingModules(v8::Isolate* isolate) {
  PendingModuleList pending_modules;
  pending_modules.swap(pending_modules_);
  for (PendingModuleList::iterator it = pending_modules.begin();
       it != pending_modules.end(); ++it) {
    AttemptToLoad(isolate, *it);
  }
}

}  // namespace gin
