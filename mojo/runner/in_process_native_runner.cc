// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/in_process_native_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "mojo/runner/native_application_support.h"

namespace mojo {
namespace runner {

InProcessNativeRunner::InProcessNativeRunner(Context* context)
    : cleanup_(shell::NativeApplicationCleanup::DONT_DELETE),
      app_library_(nullptr) {
}

InProcessNativeRunner::~InProcessNativeRunner() {
  // It is important to let the thread exit before unloading the DSO (when
  // app_library_ is destructed), because the library may have registered
  // thread-local data and destructors to run on thread termination.
  if (thread_) {
    DCHECK(thread_->HasBeenStarted());
    DCHECK(!thread_->HasBeenJoined());
    thread_->Join();
  }
}

void InProcessNativeRunner::Start(
    const base::FilePath& app_path,
    shell::NativeApplicationCleanup cleanup,
    InterfaceRequest<Application> application_request,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;
  cleanup_ = cleanup;

  DCHECK(!application_request_.is_pending());
  application_request_ = application_request.Pass();

  DCHECK(app_completed_callback_runner_.is_null());
  app_completed_callback_runner_ = base::Bind(
      &base::TaskRunner::PostTask, base::ThreadTaskRunnerHandle::Get(),
      FROM_HERE, app_completed_callback);

  DCHECK(!thread_);
  thread_.reset(new base::DelegateSimpleThread(this, "app_thread"));
  thread_->Start();
}

void InProcessNativeRunner::Run() {
  DVLOG(2) << "Loading/running Mojo app in process from library: "
           << app_path_.value()
           << " thread id=" << base::PlatformThread::CurrentId();

  // TODO(vtl): ScopedNativeLibrary doesn't have a .get() method!
  base::NativeLibrary app_library = LoadNativeApplication(app_path_, cleanup_);
  app_library_.Reset(app_library);
  RunNativeApplication(app_library, application_request_.Pass());
  app_completed_callback_runner_.Run();
  app_completed_callback_runner_.Reset();
}

scoped_ptr<shell::NativeRunner> InProcessNativeRunnerFactory::Create(
    const Options& options) {
  return make_scoped_ptr(new InProcessNativeRunner(context_));
}

}  // namespace runner
}  // namespace mojo
