// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/browser_thread_model_worker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"

using base::SingleThreadTaskRunner;

namespace syncer {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    const scoped_refptr<SingleThreadTaskRunner>& runner,
    ModelSafeGroup group)
    : runner_(runner), group_(group) {}

SyncerError BrowserThreadModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  SyncerError error = UNSET;
  if (runner_->BelongsToCurrentThread()) {
    DLOG(WARNING) << "Already on thread " << runner_;
    return work.Run();
  }

  // Signaled when the task is deleted, i.e. after it runs or when it is
  // abandoned.
  base::WaitableEvent work_done_or_abandoned(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (!runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &BrowserThreadModelWorker::CallDoWorkAndSignalTask, this, work,
              base::Passed(syncer::ScopedEventSignal(&work_done_or_abandoned)),
              &error))) {
    DLOG(WARNING) << "Failed to post task to runner " << runner_;
    error = CANNOT_DO_WORK;
    return error;
  }
  work_done_or_abandoned.Wait();
  return error;
}

ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

bool BrowserThreadModelWorker::IsOnModelThread() {
  return runner_->BelongsToCurrentThread();
}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    syncer::ScopedEventSignal scoped_event_signal,
    SyncerError* error) {
  DCHECK(runner_->BelongsToCurrentThread());
  if (!IsStopped())
    *error = work.Run();
  // The event in |scoped_event_signal| is signaled at the end of this scope.
}

}  // namespace syncer
