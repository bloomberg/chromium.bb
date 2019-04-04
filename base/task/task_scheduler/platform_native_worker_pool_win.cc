// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/platform_native_worker_pool_win.h"

#include "base/no_destructor.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/threading/thread_local.h"
#include "base/win/scoped_com_initializer.h"

namespace base {
namespace internal {

namespace {

// Used to enable COM MTA when creating threads via the Windows Thread Pool API.
ThreadLocalOwnedPointer<win::ScopedCOMInitializer>&
ScopedCOMInitializerForCurrentThread() {
  static base::NoDestructor<ThreadLocalOwnedPointer<win::ScopedCOMInitializer>>
      scoped_com_initializer;
  return *scoped_com_initializer;
}

}  // namespace

PlatformNativeWorkerPoolWin::PlatformNativeWorkerPoolWin(
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate,
    SchedulerWorkerPool* predecessor_pool)
    : PlatformNativeWorkerPool(std::move(task_tracker),
                               std::move(delegate),
                               predecessor_pool) {}

PlatformNativeWorkerPoolWin::~PlatformNativeWorkerPoolWin() {
  ::DestroyThreadpoolEnvironment(&environment_);
  ::CloseThreadpoolWork(work_);
  ::CloseThreadpool(pool_);
}

void PlatformNativeWorkerPoolWin::StartImpl() {
  ::InitializeThreadpoolEnvironment(&environment_);

  pool_ = ::CreateThreadpool(nullptr);
  DCHECK(pool_) << "LastError: " << ::GetLastError();
  ::SetThreadpoolThreadMinimum(pool_, 1);
  ::SetThreadpoolThreadMaximum(pool_, 256);

  work_ = ::CreateThreadpoolWork(&RunNextSequence, this, &environment_);
  DCHECK(work_) << "LastError: " << GetLastError();
  ::SetThreadpoolCallbackPool(&environment_, pool_);
}

void PlatformNativeWorkerPoolWin::JoinImpl() {
  ::WaitForThreadpoolWorkCallbacks(work_, true);
}

void PlatformNativeWorkerPoolWin::SubmitWork() {
  // TODO(fdoray): Handle priorities by having different work objects and using
  // SetThreadpoolCallbackPriority() and SetThreadpoolCallbackRunsLong().
  ::SubmitThreadpoolWork(work_);
}

// static
void CALLBACK PlatformNativeWorkerPoolWin::RunNextSequence(
    PTP_CALLBACK_INSTANCE,
    void* scheduler_worker_pool_windows_impl,
    PTP_WORK) {
  auto* worker_pool = static_cast<PlatformNativeWorkerPoolWin*>(
      scheduler_worker_pool_windows_impl);

  if (worker_pool->worker_environment_ == WorkerEnvironment::COM_MTA &&
      !ScopedCOMInitializerForCurrentThread().Get()) {
    ScopedCOMInitializerForCurrentThread().Set(
        std::make_unique<win::ScopedCOMInitializer>(
            win::ScopedCOMInitializer::kMTA));
  }

  worker_pool->RunNextSequenceImpl();
}

}  // namespace internal
}  // namespace base
