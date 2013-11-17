// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "gin/converter.h"

using v8::Context;
using v8::Handle;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace gin {

RunnerDelegate::RunnerDelegate() {
}

RunnerDelegate::~RunnerDelegate() {
}

Handle<ObjectTemplate> RunnerDelegate::GetGlobalTemplate(Runner* runner) {
  return Handle<ObjectTemplate>();
}

void RunnerDelegate::DidCreateContext(Runner* runner) {
}

Runner::Runner(RunnerDelegate* delegate, Isolate* isolate)
    : ContextHolder(isolate),
      delegate_(delegate) {
  HandleScope handle_scope(isolate);
  SetContext(Context::New(isolate, NULL, delegate_->GetGlobalTemplate(this)));

  v8::Context::Scope scope(context());
  delegate_->DidCreateContext(this);
}

Runner::~Runner() {
}

void Runner::Run(const std::string& script) {
  Run(Script::New(StringToV8(isolate(), script)));
}

void Runner::Run(Handle<Script> script) {
  script->Run();
}

Runner::Scope::Scope(Runner* runner)
    : handle_scope_(runner->isolate()),
      scope_(runner->context()) {
}

Runner::Scope::~Scope() {
}

}  // namespace gin
