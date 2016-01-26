// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUNNER_HOST_OUT_OF_PROCESS_NATIVE_RUNNER_H_
#define MOJO_SHELL_RUNNER_HOST_OUT_OF_PROCESS_NATIVE_RUNNER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/native_runner.h"

namespace base {
class TaskRunner;
}

namespace mojo {
namespace shell {

class ChildProcessHost;

// An implementation of |NativeRunner| that loads/runs the given app (from the
// file system) in a separate process (of its own).
class OutOfProcessNativeRunner : public NativeRunner {
 public:
  explicit OutOfProcessNativeRunner(base::TaskRunner* launch_process_runner);
  ~OutOfProcessNativeRunner() override;

  // NativeRunner:
  void Start(
      const base::FilePath& app_path,
      bool start_sandboxed,
      InterfaceRequest<Application> application_request,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& app_completed_callback) override;
  void InitHost(ScopedHandle channel,
                InterfaceRequest<Application> application_request) override;

 private:
  // |ChildController::StartApp()| callback:
  void AppCompleted(int32_t result);

  // Callback run when the child process has launched.
  void OnProcessLaunched(
      InterfaceRequest<Application> application_request,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      base::ProcessId pid);

  base::TaskRunner* const launch_process_runner_;

  base::FilePath app_path_;
  base::Closure app_completed_callback_;

  scoped_ptr<ChildProcessHost> child_process_host_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessNativeRunner);
};

class OutOfProcessNativeRunnerFactory : public NativeRunnerFactory {
 public:
  explicit OutOfProcessNativeRunnerFactory(
      base::TaskRunner* launch_process_runner)
      : launch_process_runner_(launch_process_runner) {}
  ~OutOfProcessNativeRunnerFactory() override {}

  scoped_ptr<NativeRunner> Create(const base::FilePath& app_path) override;

 private:
  base::TaskRunner* const launch_process_runner_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessNativeRunnerFactory);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_HOST_OUT_OF_PROCESS_NATIVE_RUNNER_H_
