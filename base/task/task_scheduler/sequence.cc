// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/sequence.h"

#include <utility>

#include "base/critical_closure.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Sequence::Transaction::Transaction(scoped_refptr<Sequence> sequence)
    : sequence_(sequence) {
  sequence_->lock_.Acquire();
}

Sequence::Transaction::~Transaction() {
  sequence_->lock_.AssertAcquired();
  sequence_->lock_.Release();
}

bool Sequence::Transaction::PushTask(Task task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(task.sequenced_time.is_null());
  task.sequenced_time = base::TimeTicks::Now();

  task.task = sequence_->traits_.shutdown_behavior() ==
                      TaskShutdownBehavior::BLOCK_SHUTDOWN
                  ? MakeCriticalClosure(std::move(task.task))
                  : std::move(task.task);

  sequence_->queue_.push(std::move(task));

  // Return true if the sequence was empty before the push.
  return sequence_->queue_.size() == 1;
}

Optional<Task> Sequence::Transaction::TakeTask() {
  DCHECK(!sequence_->queue_.empty());
  DCHECK(sequence_->queue_.front().task);

  return std::move(sequence_->queue_.front());
}

bool Sequence::Transaction::Pop() {
  DCHECK(!sequence_->queue_.empty());
  DCHECK(!sequence_->queue_.front().task);
  sequence_->queue_.pop();
  return sequence_->queue_.empty();
}

SequenceSortKey Sequence::Transaction::GetSortKey() const {
  DCHECK(!sequence_->queue_.empty());

  // Save the sequenced time of the next task in the sequence.
  base::TimeTicks next_task_sequenced_time =
      sequence_->queue_.front().sequenced_time;

  return SequenceSortKey(sequence_->traits_.priority(),
                         next_task_sequenced_time);
}

Sequence::Sequence(
    const TaskTraits& traits,
    scoped_refptr<SchedulerParallelTaskRunner> scheduler_parallel_task_runner)
    : traits_(traits),
      scheduler_parallel_task_runner_(scheduler_parallel_task_runner) {}

Sequence::~Sequence() {
  if (scheduler_parallel_task_runner_) {
    scheduler_parallel_task_runner_->UnregisterSequence(this);
  }
}

std::unique_ptr<Sequence::Transaction> Sequence::BeginTransaction() {
  return WrapUnique(new Transaction(this));
}

}  // namespace internal
}  // namespace base
