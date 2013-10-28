// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONTEXT_H_
#define MOJO_SHELL_CONTEXT_H_

#include "mojo/shell/loader.h"
#include "mojo/shell/storage.h"
#include "mojo/shell/task_runners.h"

namespace mojo {
namespace shell {

class Context {
 public:
  Context();
  ~Context();

  TaskRunners* task_runners() { return &task_runners_; }
  Storage* storage() { return &storage_; }
  Loader* loader() { return &loader_; }

 private:
  TaskRunners task_runners_;
  Storage storage_;
  Loader loader_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONTEXT_H_
