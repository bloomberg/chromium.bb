// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_MODULES_MODULE_RUNNER_DELEGATE_H_
#define GIN_MODULES_MODULE_RUNNER_DELEGATE_H_

#include <map>

#include "base/compiler_specific.h"
#include "gin/modules/file_module_provider.h"
#include "gin/runner.h"

namespace gin {

typedef v8::Local<v8::ObjectTemplate> (*ModuleTemplateGetter)(
    v8::Isolate* isolate);

class ModuleRunnerDelegate : public RunnerDelegate {
 public:
  explicit ModuleRunnerDelegate(const base::FilePath& module_base);
  virtual ~ModuleRunnerDelegate();

  void AddBuiltinModule(const std::string& id, ModuleTemplateGetter templ);

 private:
  typedef std::map<std::string, ModuleTemplateGetter> BuiltinModuleMap;

  // From RunnerDelegate:
  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(
      Runner* runner) OVERRIDE;
  virtual void DidCreateContext(Runner* runner) OVERRIDE;
  virtual void DidRunScript(Runner* runner,
                            v8::Handle<v8::Script> script) OVERRIDE;

  BuiltinModuleMap builtin_modules_;
  FileModuleProvider module_provider_;

  DISALLOW_COPY_AND_ASSIGN(ModuleRunnerDelegate);
};

}  // namespace gin

#endif  // GIN_MODULES_MODULE_RUNNER_DELEGATE_H_
