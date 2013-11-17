// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_RUNNER_H_
#define GIN_RUNNER_H_

#include <string>
#include "gin/context_holder.h"

namespace gin {

class Runner;

class RunnerDelegate {
 public:
  RunnerDelegate();
  virtual ~RunnerDelegate();

  // Returns the template for the global object.
  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(Runner* runner);

  virtual void DidCreateContext(Runner* runner);
};

class Runner : public ContextHolder {
 public:
  Runner(RunnerDelegate* delegate, v8::Isolate* isolate);
  ~Runner();

  void Run(const std::string& script);
  void Run(v8::Handle<v8::Script> script);

  v8::Handle<v8::Object> global() const {
    return context()->Global();
  }

  class Scope {
   public:
    explicit Scope(Runner* runner);
    ~Scope();

   private:
    v8::HandleScope handle_scope_;
    v8::Context::Scope scope_;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

 private:
  friend class Scope;

  RunnerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

}  // namespace gin

#endif  // GIN_RUNNER_H_
