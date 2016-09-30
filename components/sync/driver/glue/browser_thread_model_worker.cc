// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/browser_thread_model_worker.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"

using base::SingleThreadTaskRunner;
using base::WaitableEvent;

namespace syncer {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    const scoped_refptr<SingleThreadTaskRunner>& runner,
    ModelSafeGroup group,
    WorkerLoopDestructionObserver* observer)
    : ModelSafeWorker(observer), runner_(runner), group_(group) {}

SyncerError BrowserThreadModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  SyncerError error = UNSET;
  if (runner_->BelongsToCurrentThread()) {
    DLOG(WARNING) << "Already on thread " << runner_;
    return work.Run();
  }

  if (!runner_->PostTask(
          FROM_HERE,
          base::Bind(&BrowserThreadModelWorker::CallDoWorkAndSignalTask, this,
                     work, work_done_or_stopped(), &error))) {
    DLOG(WARNING) << "Failed to post task to runner " << runner_;
    error = CANNOT_DO_WORK;
    return error;
  }
  work_done_or_stopped()->Wait();
  return error;
}

ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

void BrowserThreadModelWorker::RegisterForLoopDestruction() {
  if (runner_->BelongsToCurrentThread()) {
    SetWorkingLoopToCurrent();
  } else {
    runner_->PostTask(
        FROM_HERE,
        Bind(&BrowserThreadModelWorker::RegisterForLoopDestruction, this));
  }
}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(const WorkCallback& work,
                                                       WaitableEvent* done,
                                                       SyncerError* error) {
  DCHECK(runner_->BelongsToCurrentThread());
  if (!IsStopped())
    *error = work.Run();
  done->Signal();
}

}  // namespace syncer
