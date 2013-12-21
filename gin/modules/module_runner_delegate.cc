// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_runner_delegate.h"

#include "gin/modules/module_registry.h"
#include "gin/object_template_builder.h"

namespace gin {

ModuleRunnerDelegate::ModuleRunnerDelegate(
  const std::vector<base::FilePath>& search_paths)
    : module_provider_(search_paths) {
}

ModuleRunnerDelegate::~ModuleRunnerDelegate() {
}

void ModuleRunnerDelegate::AddBuiltinModule(const std::string& id,
                                            ModuleGetter getter) {
  builtin_modules_[id] = getter;
}

void ModuleRunnerDelegate::AttemptToLoadMoreModules(Runner* runner) {
  ModuleRegistry* registry = ModuleRegistry::From(runner->context());
  registry->AttemptToLoadMoreModules(runner->isolate());
  module_provider_.AttempToLoadModules(
      runner, registry->unsatisfied_dependencies());
}

v8::Handle<v8::ObjectTemplate> ModuleRunnerDelegate::GetGlobalTemplate(
    Runner* runner) {
  v8::Handle<v8::ObjectTemplate> templ =
      ObjectTemplateBuilder(runner->isolate()).Build();
  ModuleRegistry::RegisterGlobals(runner->isolate(), templ);
  return templ;
}

void ModuleRunnerDelegate::DidCreateContext(Runner* runner) {
  RunnerDelegate::DidCreateContext(runner);

  v8::Handle<v8::Context> context = runner->context();
  ModuleRegistry* registry = ModuleRegistry::From(context);

  for (BuiltinModuleMap::const_iterator it = builtin_modules_.begin();
       it != builtin_modules_.end(); ++it) {
    registry->AddBuiltinModule(runner->isolate(), it->first,
                               it->second(runner->isolate()));
  }
}

void ModuleRunnerDelegate::DidRunScript(Runner* runner) {
  AttemptToLoadMoreModules(runner);
}

}  // namespace gin
