// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/out_of_process_native_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "mojo/runner/child_process_host.h"
#include "mojo/runner/in_process_native_runner.h"

namespace mojo {
namespace runner {

OutOfProcessNativeRunner::OutOfProcessNativeRunner(Context* context)
    : context_(context) {
}

OutOfProcessNativeRunner::~OutOfProcessNativeRunner() {
  if (child_process_host_)
    child_process_host_->Join();
}

void OutOfProcessNativeRunner::Start(
    const base::FilePath& app_path,
    bool start_sandboxed,
    InterfaceRequest<Application> application_request,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(app_completed_callback_.is_null());
  app_completed_callback_ = app_completed_callback;

  child_process_host_.reset(
      new ChildProcessHost(context_, start_sandboxed, app_path));
  child_process_host_->Start();

  child_process_host_->StartApp(
      application_request.Pass(),
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

scoped_ptr<shell::NativeRunner> OutOfProcessNativeRunnerFactory::Create(
    const base::FilePath& app_path) {
  return make_scoped_ptr(new OutOfProcessNativeRunner(context_));
}

}  // namespace runner
}  // namespace mojo
