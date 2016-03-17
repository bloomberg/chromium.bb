// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/sequence_sort_key.h"

namespace base {
namespace internal {

SequenceSortKey::SequenceSortKey(TaskPriority priority,
                                 TimeTicks next_task_sequenced_time)
    : priority(priority), next_task_sequenced_time(next_task_sequenced_time) {}

bool SequenceSortKey::operator<(const SequenceSortKey& other) const {
  // This SequenceSortKey is considered less important than |other| if it has a
  // lower priority or if it has the same priority but its next task was posted
  // later than |other|'s.
  const int priority_diff =
      static_cast<int>(priority) - static_cast<int>(other.priority);
  if (priority_diff < 0)
    return true;
  if (priority_diff > 0)
    return false;
  return next_task_sequenced_time > other.next_task_sequenced_time;
}

}  // namespace internal
}  // namespace base
