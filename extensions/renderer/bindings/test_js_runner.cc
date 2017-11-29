// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/test_js_runner.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"

namespace extensions {

namespace {

// NOTE(devlin): This isn't thread-safe. If we have multi-threaded unittests,
// we'll need to expand this.
bool g_allow_errors = false;

}  // namespace

TestJSRunner::Scope::Scope(std::unique_ptr<JSRunner> runner)
    : runner_(std::move(runner)),
      old_runner_(JSRunner::GetInstanceForTesting()) {
  DCHECK_NE(runner_.get(), old_runner_);
  JSRunner::SetInstanceForTesting(runner_.get());
}

TestJSRunner::Scope::~Scope() {
  DCHECK_EQ(runner_.get(), JSRunner::GetInstanceForTesting());
  JSRunner::SetInstanceForTesting(old_runner_);
}

TestJSRunner::AllowErrors::AllowErrors() {
  DCHECK(!g_allow_errors) << "Nested AllowErrors() blocks are not allowed.";
  g_allow_errors = true;
}

TestJSRunner::AllowErrors::~AllowErrors() {
  DCHECK(g_allow_errors);
  g_allow_errors = false;
}

TestJSRunner::TestJSRunner() {}
TestJSRunner::TestJSRunner(const base::Closure& will_call_js)
    : will_call_js_(will_call_js) {}
TestJSRunner::~TestJSRunner() {}

void TestJSRunner::RunJSFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 int argc,
                                 v8::Local<v8::Value> argv[]) {
  if (will_call_js_)
    will_call_js_.Run();

  if (g_allow_errors)
    ignore_result(function->Call(context, context->Global(), argc, argv));
  else
    RunFunctionOnGlobal(function, context, argc, argv);
}

v8::Global<v8::Value> TestJSRunner::RunJSFunctionSync(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  if (will_call_js_)
    will_call_js_.Run();

  if (g_allow_errors) {
    v8::Global<v8::Value> global_result;
    v8::Local<v8::Value> result;
    if (function->Call(context, context->Global(), argc, argv).ToLocal(&result))
      global_result.Reset(context->GetIsolate(), result);
    return global_result;
  }
  return RunFunctionOnGlobalAndReturnHandle(function, context, argc, argv);
}

}  // namespace extensions
