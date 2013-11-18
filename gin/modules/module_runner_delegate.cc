// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_runner_delegate.h"

#include "gin/modules/module_registry.h"

namespace gin {

ModuleRunnerDelegate::ModuleRunnerDelegate(const base::FilePath& module_base)
    : module_provider_(module_base) {
}

ModuleRunnerDelegate::~ModuleRunnerDelegate() {
}

v8::Handle<v8::ObjectTemplate> ModuleRunnerDelegate::GetGlobalTemplate(
    Runner* runner) {
  v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New();
  ModuleRegistry::RegisterGlobals(runner->isolate(), templ);
  return templ;
}

void ModuleRunnerDelegate::DidRunScript(Runner* runner,
                                        v8::Handle<v8::Script> script) {
  ModuleRegistry* registry = ModuleRegistry::From(runner->context());
  registry->AttemptToLoadMoreModules(runner->isolate());
  module_provider_.AttempToLoadModules(
      runner, registry->unsatisfied_dependencies());
}

}  // namespace gin
