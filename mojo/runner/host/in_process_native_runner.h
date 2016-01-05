// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_
#define MOJO_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/threading/simple_thread.h"
#include "mojo/runner/host/native_application_support.h"
#include "mojo/shell/native_runner.h"

namespace base {
class TaskRunner;
}

namespace mojo {
namespace runner {

// An implementation of |NativeRunner| that loads/runs the given app (from the
// file system) on a separate thread (in the current process).
class InProcessNativeRunner : public shell::NativeRunner,
                              public base::DelegateSimpleThread::Delegate {
 public:
  InProcessNativeRunner();
  ~InProcessNativeRunner() override;

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
  // |base::DelegateSimpleThread::Delegate| method:
  void Run() override;

  base::FilePath app_path_;
  InterfaceRequest<Application> application_request_;
  base::Callback<bool(void)> app_completed_callback_runner_;

  base::ScopedNativeLibrary app_library_;
  scoped_ptr<base::DelegateSimpleThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunner);
};

class InProcessNativeRunnerFactory : public shell::NativeRunnerFactory {
 public:
  explicit InProcessNativeRunnerFactory(base::TaskRunner* launch_process_runner)
      : launch_process_runner_(launch_process_runner) {}
  ~InProcessNativeRunnerFactory() override {}

  scoped_ptr<shell::NativeRunner> Create(
      const base::FilePath& app_path) override;

 private:
  base::TaskRunner* const launch_process_runner_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunnerFactory);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_
