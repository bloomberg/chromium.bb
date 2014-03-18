// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_IN_PROCESS_DYNAMIC_SERVICE_RUNNER_H_
#define MOJO_SHELL_IN_PROCESS_DYNAMIC_SERVICE_RUNNER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "mojo/shell/dynamic_service_runner.h"

namespace mojo {
namespace shell {

class InProcessDynamicServiceRunner
    : public DynamicServiceRunner,
      public base::DelegateSimpleThread::Delegate {
 public:
  explicit InProcessDynamicServiceRunner(Context* context);
  virtual ~InProcessDynamicServiceRunner();

  // |DynamicServiceRunner| method:
  virtual void Start(const base::FilePath& app_path,
                     ScopedShellHandle service_handle,
                     const base::Closure& app_completed_callback) OVERRIDE;

 private:
  // |base::DelegateSimpleThread::Delegate| method:
  virtual void Run() OVERRIDE;

  base::FilePath app_path_;
  ScopedShellHandle service_handle_;
  base::Closure app_completed_callback_;

  base::DelegateSimpleThread thread_;

  DISALLOW_COPY_AND_ASSIGN(InProcessDynamicServiceRunner);
};

typedef DynamicServiceRunnerFactoryImpl<InProcessDynamicServiceRunner>
    InProcessDynamicServiceRunnerFactory;

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_IN_PROCESS_DYNAMIC_SERVICE_RUNNER_H_
