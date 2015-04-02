// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_APP_CHILD_PROCESS_H_
#define SHELL_APP_CHILD_PROCESS_H_

#include "base/macros.h"
#include "mojo/shell/child_process.h"

namespace mojo {
namespace shell {

// An implementation of |ChildProcess| for an app child process, which runs a
// single app (loaded from the file system) on its main thread.
class AppChildProcess : public ChildProcess {
 public:
  AppChildProcess();
  ~AppChildProcess() override;

  void Main() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppChildProcess);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_APP_CHILD_PROCESS_H_
