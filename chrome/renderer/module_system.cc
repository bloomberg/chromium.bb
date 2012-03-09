// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/module_system.h"

#include "base/bind.h"
#include "chrome/renderer/static_v8_external_string_resource.h"
#include "grit/renderer_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

v8::Handle<v8::String> GetResource(int resourceId) {
  const ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  return v8::String::NewExternal(new StaticV8ExternalAsciiStringResource(
      resource_bundle.GetRawDataResource(resourceId)));
}

} // namespace

ModuleSystem::ModuleSystem(const std::map<std::string, std::string>* source_map)
    : source_map_(source_map) {
  RouteFunction("Run",
      base::Bind(&ModuleSystem::Run, base::Unretained(this)));
  RouteFunction("GetSource",
      base::Bind(&ModuleSystem::GetSource, base::Unretained(this)));
  RouteFunction("GetNative",
      base::Bind(&ModuleSystem::GetNative, base::Unretained(this)));
}

ModuleSystem::~ModuleSystem() {
}

void ModuleSystem::Require(const std::string& module_name) {
  EnsureRequireLoaded();
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[] = {
    v8::String::New(module_name.c_str()),
  };
  require_->Call(v8::Context::GetCurrent()->Global(), arraysize(argv), argv);
}

void ModuleSystem::RegisterNativeHandler(const std::string& name,
    scoped_ptr<NativeHandler> native_handler) {
  native_handler_map_[name] =
      linked_ptr<NativeHandler>(native_handler.release());
}

void ModuleSystem::EnsureRequireLoaded() {
  v8::HandleScope handle_scope;
  if (!require_.IsEmpty())
    return;
  v8::Handle<v8::Object> bootstrap = NewInstance();
  v8::Handle<v8::Value> result = RunString(GetResource(IDR_REQUIRE_JS));
  v8::Handle<v8::Function> require_factory =
      v8::Handle<v8::Function>::Cast(result);
  CHECK(!require_factory.IsEmpty())
      << "require.js should define a function that returns a require() "
      << "function";
  v8::Handle<v8::Value> argv[] = {
    bootstrap,
  };
  v8::Handle<v8::Value> require = require_factory->Call(
      v8::Context::GetCurrent()->Global(), arraysize(argv), argv);
  require_ = v8::Persistent<v8::Function>::New(
      v8::Handle<v8::Function>::Cast(require));
}

v8::Handle<v8::Value> ModuleSystem::RunString(v8::Handle<v8::String> code) {
  v8::HandleScope handle_scope;
  return handle_scope.Close(v8::Script::New(code)->Run());
}

v8::Handle<v8::Value> ModuleSystem::Run(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  v8::HandleScope handle_scope;
  return handle_scope.Close(v8::Script::New(args[0]->ToString())->Run());
}

v8::Handle<v8::Value> ModuleSystem::GetSource(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  std::string module_name = *v8::String::AsciiValue(args[0]->ToString());
  std::map<std::string, std::string>::const_iterator p =
      source_map_->find(module_name);
  if (p == source_map_->end())
    return v8::Undefined();
  return v8::String::New(p->second.c_str());
}

v8::Handle<v8::Value> ModuleSystem::GetNative(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  std::string native_name = *v8::String::AsciiValue(args[0]->ToString());
  NativeHandlerMap::iterator i = native_handler_map_.find(native_name);
  if (i == native_handler_map_.end())
    return v8::Undefined();
  return i->second->NewInstance();
}
