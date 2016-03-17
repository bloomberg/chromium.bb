// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SEQUENCE_H_
#define BASE_TASK_SCHEDULER_SEQUENCE_H_

#include <stddef.h>

#include <queue>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"

namespace base {
namespace internal {

// A sequence holds tasks that must be executed in posting order.
// This class is thread-safe.
class BASE_EXPORT Sequence : public RefCountedThreadSafe<Sequence> {
 public:
  Sequence();

  // Adds |task| at the end of the sequence's queue. Returns true if the
  // sequence was empty before this operation.
  bool PushTask(scoped_ptr<Task> task);

  // Returns the task in front of the sequence's queue, if any.
  const Task* PeekTask() const;

  // Removes the task in front of the sequence's queue. Returns true if the
  // sequence is empty after this operation. Cannot be called on an empty
  // sequence.
  bool PopTask();

  // Returns a SequenceSortKey representing the priority of the sequence. Cannot
  // be called on an empty sequence.
  SequenceSortKey GetSortKey() const;

 private:
  friend class RefCountedThreadSafe<Sequence>;
  ~Sequence();

  // Synchronizes access to all members.
  mutable SchedulerLock lock_;

  // Queue of tasks to execute.
  std::queue<scoped_ptr<Task>> queue_;

  // Number of tasks contained in the sequence for each priority.
  size_t num_tasks_per_priority_[static_cast<int>(TaskPriority::HIGHEST) + 1] =
      {};

  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SEQUENCE_H_
