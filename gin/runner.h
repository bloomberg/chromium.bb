// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_RUNNER_H_
#define GIN_RUNNER_H_

#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace gin {

class Runner;

class RunnerDelegate {
 public:
  // Returns the object that is passed to the script's |main| function.
  virtual v8::Handle<v8::Object> CreateRootObject(Runner* runner) = 0;

 protected:
  virtual ~RunnerDelegate();
};

class Runner {
 public:
  Runner(RunnerDelegate* delegate, v8::Isolate* isolate);
  ~Runner();

  void Run(v8::Handle<v8::Script> script);

  v8::Isolate* isolate() const { return isolate_; }

  v8::Handle<v8::Context> context() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

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

  v8::Handle<v8::Function> GetMain();

  RunnerDelegate* delegate_;
  v8::Isolate* isolate_;
  v8::Persistent<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

}  // namespace gin

#endif  // GIN_RUNNER_H_
