// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequenced_worker_pool_task_runner.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"

namespace base {

SequencedWorkerPoolTaskRunner::SequencedWorkerPoolTaskRunner(
    const scoped_refptr<SequencedWorkerPool>& pool,
    SequencedWorkerPool::SequenceToken token)
    : pool_(pool),
      token_(token) {
}

SequencedWorkerPoolTaskRunner::~SequencedWorkerPoolTaskRunner() {
}

bool SequencedWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  return PostDelayedTaskAssertZeroDelay(from_here, task, delay);
}

bool SequencedWorkerPoolTaskRunner::RunsTasksOnCurrentThread() const {
  return pool_->IsRunningSequenceOnCurrentThread(token_);
}

bool SequencedWorkerPoolTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  return PostDelayedTaskAssertZeroDelay(from_here, task, delay);
}

bool SequencedWorkerPoolTaskRunner::PostDelayedTaskAssertZeroDelay(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  // TODO(francoisk777@gmail.com): Change the following two statements once
  // SequencedWorkerPool supports non-zero delays.
  DCHECK_EQ(delay.InMillisecondsRoundedUp(), 0)
      << "SequencedWorkerPoolTaskRunner does not yet support non-zero delays";
  return pool_->PostSequencedWorkerTask(token_, from_here, task);
}

}  // namespace base
