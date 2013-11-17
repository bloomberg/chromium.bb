// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/runner_delegate.h"

#include "gin/modules/module_registry.h"
#include "mojo/public/bindings/js/core.h"
#include "mojo/public/bindings/js/global.h"

namespace mojo {
namespace js {

RunnerDelegate::RunnerDelegate() {
}

RunnerDelegate::~RunnerDelegate() {
}

v8::Handle<v8::ObjectTemplate> RunnerDelegate::GetGlobalTemplate(
    gin::Runner* runner) {
  return js::GetGlobalTemplate(runner->isolate());
}

void RunnerDelegate::DidCreateContext(gin::Runner* runner) {
  v8::Handle<v8::Context> context = runner->context();
  gin::ModuleRegistry* registry =
      gin::ModuleRegistry::From(context);

  registry->AddBuiltinModule(runner->isolate(), "core",
                             GetCoreTemplate(runner->isolate()));
}

}  // namespace js
}  // namespace mojo
