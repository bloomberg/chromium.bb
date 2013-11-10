// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "gin/converter.h"

using v8::Context;
using v8::Function;
using v8::Handle;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

namespace gin {

RunnerDelegate::~RunnerDelegate() {
}

Runner::Runner(RunnerDelegate* delegate, Isolate* isolate)
    : delegate_(delegate),
      isolate_(isolate) {
  HandleScope handle_scope(isolate_);
  context_.Reset(isolate_, Context::New(isolate_));
}

Runner::~Runner() {
  // TODO(abarth): Figure out how to set kResetInDestructor to true.
  context_.Reset();
}

void Runner::Run(Handle<Script> script) {
  script->Run();
  Handle<Function> main = GetMain();
  if (main.IsEmpty())
    return;
  Handle<Value> argv[] = { delegate_->CreateRootObject(this) };
  main->Call(global(), 1, argv);
}

v8::Handle<v8::Function> Runner::GetMain() {
  Handle<Value> property = global()->Get(StringToV8(isolate_, "main"));
  if (property.IsEmpty())
    return v8::Handle<v8::Function>();
  Handle<Function> main;
  if (!ConvertFromV8(property, &main))
    return v8::Handle<v8::Function>();
  return main;
}

Runner::Scope::Scope(Runner* runner)
  : handle_scope_(runner->isolate_),
    scope_(Local<Context>::New(runner->isolate_, runner->context_)) {
}

Runner::Scope::~Scope() {
}

}  // namespace gin
