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
    base::TaskRunner* launch_process_runner,
    NativeRunnerDelegate* delegate)
    : launch_process_runner_(launch_process_runner), delegate_(delegate) {}

OutOfProcessNativeRunner::~OutOfProcessNativeRunner() {
  if (child_process_host_ && !app_path_.empty())
    child_process_host_->Join();
}

void OutOfProcessNativeRunner::Start(
    const base::FilePath& app_path,
    const Identity& target,
    bool start_sandboxed,
    mojom::ShellClientRequest request,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(app_completed_callback_.is_null());
  app_completed_callback_ = app_completed_callback;

  child_process_host_.reset(new ChildProcessHost(
      launch_process_runner_, delegate_, start_sandboxed, target, app_path));
  child_process_host_->Start(base::Bind(
      &OutOfProcessNativeRunner::OnProcessLaunched, base::Unretained(this),
      base::Passed(&request), target.name(), pid_available_callback));
}

void OutOfProcessNativeRunner::InitHost(
    mojom::ShellClientFactoryPtr factory,
    const String& name,
    mojom::ShellClientRequest request) {
  child_process_host_.reset(new ChildProcessHost(std::move(factory)));
  child_process_host_->StartChild(
      std::move(request), name,
      base::Bind(&OutOfProcessNativeRunner::AppCompleted,
                 base::Unretained(this)));
}

void OutOfProcessNativeRunner::AppCompleted() {
  if (child_process_host_)
    child_process_host_->Join();
  child_process_host_.reset();
  // This object may be deleted by this callback.
  base::Closure app_completed_callback = app_completed_callback_;
  app_completed_callback_.Reset();
  if (!app_completed_callback.is_null())
    app_completed_callback.Run();
}

void OutOfProcessNativeRunner::OnProcessLaunched(
    mojom::ShellClientRequest request,
    const String& name,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    base::ProcessId pid) {
  DCHECK(child_process_host_);
  child_process_host_->StartChild(
      std::move(request), name,
      base::Bind(&OutOfProcessNativeRunner::AppCompleted,
                 base::Unretained(this)));
  pid_available_callback.Run(pid);
}

OutOfProcessNativeRunnerFactory::OutOfProcessNativeRunnerFactory(
    base::TaskRunner* launch_process_runner,
    NativeRunnerDelegate* delegate)
    : launch_process_runner_(launch_process_runner), delegate_(delegate) {}
OutOfProcessNativeRunnerFactory::~OutOfProcessNativeRunnerFactory() {}

scoped_ptr<shell::NativeRunner> OutOfProcessNativeRunnerFactory::Create(
    const base::FilePath& app_path) {
  return make_scoped_ptr(
      new OutOfProcessNativeRunner(launch_process_runner_, delegate_));
}

}  // namespace shell
}  // namespace mojo
