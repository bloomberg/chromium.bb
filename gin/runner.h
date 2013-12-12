// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_RUNNER_H_
#define GIN_RUNNER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "gin/gin_export.h"
#include "gin/public/context_holder.h"

namespace gin {

class Runner;
class TryCatch;

// Subclass RunnerDelegate to customize the behavior of |Runner|. Typical
// embedders will want to subclass one of the specialized RunnerDelegates,
// such as ModuleRunnerDelegate.
class GIN_EXPORT RunnerDelegate {
 public:
  RunnerDelegate();
  virtual ~RunnerDelegate();

  // Returns the template for the global object.
  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(Runner* runner);
  virtual void DidCreateContext(Runner* runner);
  virtual void WillRunScript(Runner* runner);
  virtual void DidRunScript(Runner* runner);
  virtual void UnhandledException(Runner* runner, TryCatch& try_catch);
};

// Runner lets you run code in a v8::Context. Upon construction, Runner will
// create a v8::Context. Upon destruction, Runner will dispose the context.
class GIN_EXPORT Runner : public ContextHolder {
 public:
  Runner(RunnerDelegate* delegate, v8::Isolate* isolate);
  ~Runner();

  // Before running script in this context, you'll need to enter the runner's
  // context by creating an instance of Runner::Scope on the stack.
  void Run(const std::string& source, const std::string& resource_name);
  void Run(v8::Handle<v8::Script> script);

  v8::Handle<v8::Value> Call(v8::Handle<v8::Function> function,
                             v8::Handle<v8::Value> receiver,
                             int argc,
                             v8::Handle<v8::Value> argv[]);

  v8::Handle<v8::Object> global() const {
    return context()->Global();
  }

  // Useful for running script in this context asynchronously. Rather than
  // holding a raw pointer to the runner, consider holding a WeakPtr.
  base::WeakPtr<Runner> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  class GIN_EXPORT Scope {
   public:
    explicit Scope(Runner* runner);
    ~Scope();

   private:
    v8::Isolate::Scope isolate_scope_;
    v8::HandleScope handle_scope_;
    v8::Context::Scope scope_;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

 private:
  friend class Scope;

  RunnerDelegate* delegate_;

  base::WeakPtrFactory<Runner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

}  // namespace gin

#endif  // GIN_RUNNER_H_
