// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/runner/host/out_of_process_native_runner.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "mojo/shell/runner/host/child_process_host.h"
#include "mojo/shell/runner/host/in_process_native_runner.h"

namespace mojo {
namespace shell {

OutOfProcessNativeRunner::OutOfProcessNativeRunner(
    base::TaskRunner* launch_process_runner)
    : launch_process_runner_(launch_process_runner) {}

OutOfProcessNativeRunner::~OutOfProcessNativeRunner() {
  if (child_process_host_ && !app_path_.empty())
    child_process_host_->Join();
}

void OutOfProcessNativeRunner::Start(
    const base::FilePath& app_path,
    bool start_sandboxed,
    InterfaceRequest<Application> application_request,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(app_completed_callback_.is_null());
  app_completed_callback_ = app_completed_callback;

  child_process_host_.reset(
      new ChildProcessHost(launch_process_runner_, start_sandboxed, app_path));
  child_process_host_->Start(base::Bind(
      &OutOfProcessNativeRunner::OnProcessLaunched, base::Unretained(this),
      base::Passed(&application_request), pid_available_callback));
}

void OutOfProcessNativeRunner::InitHost(
    ScopedHandle channel,
    InterfaceRequest<Application> application_request) {
  child_process_host_.reset(new ChildProcessHost(std::move(channel)));
  child_process_host_->StartApp(
      std::move(application_request),
      base::Bind(&OutOfProcessNativeRunner::AppCompleted,
                 base::Unretained(this)));
}

void OutOfProcessNativeRunner::AppCompleted(int32_t result) {
  DVLOG(2) << "OutOfProcessNativeRunner::AppCompleted(" << result << ")";

  if (child_process_host_)
    child_process_host_->Join();
  child_process_host_.reset();
  // This object may be deleted by this callback.
  base::Closure app_completed_callback = app_completed_callback_;
  app_completed_callback_.Reset();
  app_completed_callback.Run();
}

void OutOfProcessNativeRunner::OnProcessLaunched(
    InterfaceRequest<Application> application_request,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    base::ProcessId pid) {
  DCHECK(child_process_host_);
  child_process_host_->StartApp(
      std::move(application_request),
      base::Bind(&OutOfProcessNativeRunner::AppCompleted,
                 base::Unretained(this)));
  pid_available_callback.Run(pid);
}

scoped_ptr<shell::NativeRunner> OutOfProcessNativeRunnerFactory::Create(
    const base::FilePath& app_path) {
  return make_scoped_ptr(new OutOfProcessNativeRunner(launch_process_runner_));
}

}  // namespace shell
}  // namespace mojo
