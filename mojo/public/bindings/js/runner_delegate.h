// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_JS_RUNNER_H_
#define MOJO_PUBLIC_BINDINGS_JS_RUNNER_H_

#include "gin/runner.h"
#include "mojo/public/system/macros.h"

namespace mojo {
namespace js {

class RunnerDelegate : public gin::RunnerDelegate {
 public:
  RunnerDelegate();
  virtual ~RunnerDelegate();

  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(
      gin::Runner* runner) MOJO_OVERRIDE;

  virtual void DidCreateContext(gin::Runner* runner) MOJO_OVERRIDE;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(RunnerDelegate);
};

}  // namespace js
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_JS_RUNNER_H_
