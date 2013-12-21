// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "gin/converter.h"
#include "gin/per_context_data.h"
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

void RunnerDelegate::WillRunScript(Runner* runner) {
}

void RunnerDelegate::DidRunScript(Runner* runner) {
}

void RunnerDelegate::UnhandledException(Runner* runner, TryCatch& try_catch) {
  CHECK(false) << try_catch.GetStackTrace();
}

Runner::Runner(RunnerDelegate* delegate, Isolate* isolate)
    : ContextHolder(isolate),
      delegate_(delegate),
      weak_factory_(this) {
  v8::Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context =
      Context::New(isolate, NULL, delegate_->GetGlobalTemplate(this));

  SetContext(context);
  PerContextData::From(context)->set_runner(this);

  v8::Context::Scope scope(context);
  delegate_->DidCreateContext(this);
}

Runner::~Runner() {
}

void Runner::Run(const std::string& source, const std::string& resource_name) {
  TryCatch try_catch;
  v8::Handle<Script> script = Script::New(StringToV8(isolate(), source),
                                          StringToV8(isolate(), resource_name));
  if (try_catch.HasCaught()) {
    delegate_->UnhandledException(this, try_catch);
    return;
  }

  Run(script);
}

void Runner::Run(v8::Handle<Script> script) {
  TryCatch try_catch;
  delegate_->WillRunScript(this);

  script->Run();

  delegate_->DidRunScript(this);
  if (try_catch.HasCaught()) {
    delegate_->UnhandledException(this, try_catch);
  }
}

v8::Handle<v8::Value> Runner::Call(v8::Handle<v8::Function> function,
                                   v8::Handle<v8::Value> receiver,
                                   int argc,
                                   v8::Handle<v8::Value> argv[]) {
  TryCatch try_catch;
  delegate_->WillRunScript(this);

  v8::Handle<v8::Value> result = function->Call(receiver, argc, argv);

  delegate_->DidRunScript(this);
  if (try_catch.HasCaught()) {
    delegate_->UnhandledException(this, try_catch);
  }

  return result;
}

Runner::Scope::Scope(Runner* runner)
    : isolate_scope_(runner->isolate()),
      handle_scope_(runner->isolate()),
      scope_(runner->context()) {
}

Runner::Scope::~Scope() {
}

}  // namespace gin
