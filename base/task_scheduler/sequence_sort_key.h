// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SEQUENCE_SORT_KEY_H_
#define BASE_TASK_SCHEDULER_SEQUENCE_SORT_KEY_H_

#include "base/base_export.h"
#include "base/task_scheduler/task_traits.h"
#include "base/time/time.h"

namespace base {
namespace internal {

// An immutable representation of the priority of a Sequence.
struct BASE_EXPORT SequenceSortKey final {
  SequenceSortKey(TaskPriority priority, TimeTicks next_task_sequenced_time);

  bool operator<(const SequenceSortKey& other) const;
  bool operator>(const SequenceSortKey& other) const { return other < *this; }

  // Highest task priority in the sequence at the time this sort key was
  // created.
  const TaskPriority priority;

  // Sequenced time of the next task to run in the sequence at the time this
  // sort key was created.
  const TimeTicks next_task_sequenced_time;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SEQUENCE_SORT_KEY_H_
