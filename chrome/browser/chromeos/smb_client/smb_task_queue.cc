// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_task_queue.h"

#include "base/bind.h"
#include "base/callback.h"

namespace chromeos {
namespace smb_client {

SmbTaskQueue::SmbTaskQueue(size_t max_pending) : max_pending_(max_pending) {}
SmbTaskQueue::~SmbTaskQueue() {}

void SmbTaskQueue::AddTask(SmbTask task) {
  operations_.push(std::move(task));
  RunTaskIfNeccessary();
}

void SmbTaskQueue::TaskFinished() {
  DCHECK_GT(num_pending_, 0u);
  --num_pending_;
  RunTaskIfNeccessary();
}

void SmbTaskQueue::RunTaskIfNeccessary() {
  if (IsCapacityToRunTask() && IsTaskToRun()) {
    RunNextTask();

    // Sanity check that either the maximum number of tasks are running or
    // nothing is in the queue. If there is anything left in the queue to run,
    // then the maximum number of tasks should already be running.
    DCHECK(!IsCapacityToRunTask() || !IsTaskToRun());
  }
}

SmbTask SmbTaskQueue::GetNextTask() {
  DCHECK(IsTaskToRun());

  SmbTask next_task = std::move(operations_.front());
  operations_.pop();
  return next_task;
}

void SmbTaskQueue::RunNextTask() {
  DCHECK(IsTaskToRun());

  ++num_pending_;
  GetNextTask().Run();
}

bool SmbTaskQueue::IsTaskToRun() const {
  return !operations_.empty();
}

bool SmbTaskQueue::IsCapacityToRunTask() const {
  return num_pending_ < max_pending_;
}

}  // namespace smb_client
}  // namespace chromeos
