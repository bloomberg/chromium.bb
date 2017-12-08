// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_SHELL_RUNNER_H_
#define GIN_SHELL_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "gin/runner.h"

namespace gin {

class ContextHolder;
class ShellRunner;
class TryCatch;

// Subclass ShellRunnerDelegate to customize the behavior of ShellRunner.
// Typical embedders will want to subclass one of the specialized
// ShellRunnerDelegates, such as ModuleRunnerDelegate.
class GIN_EXPORT ShellRunnerDelegate {
 public:
  ShellRunnerDelegate();
  virtual ~ShellRunnerDelegate();

  // Returns the template for the global object.
  virtual v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      ShellRunner* runner,
      v8::Isolate* isolate);
  virtual void DidCreateContext(ShellRunner* runner);
  virtual void WillRunScript(ShellRunner* runner);
  virtual void DidRunScript(ShellRunner* runner);
  virtual void UnhandledException(ShellRunner* runner, TryCatch& try_catch);
};

// ShellRunner executes the script/functions directly in a v8::Context.
// ShellRunner owns a ContextHolder and v8::Context, both of which are destroyed
// when the ShellRunner is deleted.
class GIN_EXPORT ShellRunner : public Runner {
 public:
  ShellRunner(ShellRunnerDelegate* delegate, v8::Isolate* isolate);
  ~ShellRunner() override;

  // Before running script in this context, you'll need to enter the runner's
  // context by creating an instance of Runner::Scope on the stack.

  // Runner overrides:
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Local<v8::Value> Call(v8::Local<v8::Function> function,
                             v8::Local<v8::Value> receiver,
                             int argc,
                             v8::Local<v8::Value> argv[]) override;
  ContextHolder* GetContextHolder() override;

 private:
  friend class Scope;

  void Run(v8::Local<v8::Script> script);

  ShellRunnerDelegate* delegate_;

  std::unique_ptr<ContextHolder> context_holder_;

  DISALLOW_COPY_AND_ASSIGN(ShellRunner);
};

}  // namespace gin

#endif  // GIN_SHELL_RUNNER_H_
