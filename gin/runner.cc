// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "gin/converter.h"
#include "gin/try_catch.h"

using v8::Context;
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

v8::Handle<ObjectTemplate> RunnerDelegate::GetGlobalTemplate(Runner* runner) {
  return v8::Handle<ObjectTemplate>();
}

void RunnerDelegate::DidCreateContext(Runner* runner) {
}

void RunnerDelegate::WillRunScript(Runner* runner, v8::Handle<Script> script) {
}

void RunnerDelegate::DidRunScript(Runner* runner, v8::Handle<Script> script) {
}

void RunnerDelegate::UnhandledException(Runner* runner, TryCatch& try_catch) {
}

Runner::Runner(RunnerDelegate* delegate, Isolate* isolate)
    : ContextHolder(isolate),
      delegate_(delegate),
      weak_factory_(this) {
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

void Runner::Run(v8::Handle<Script> script) {
  delegate_->WillRunScript(this, script);
  {
    TryCatch try_catch;
    script->Run();
    if (try_catch.HasCaught())
      delegate_->UnhandledException(this, try_catch);
  }
  delegate_->DidRunScript(this, script);
}

Runner::Scope::Scope(Runner* runner)
    : handle_scope_(runner->isolate()),
      scope_(runner->context()) {
}

Runner::Scope::~Scope() {
}

}  // namespace gin
