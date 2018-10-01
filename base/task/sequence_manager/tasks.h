// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASKS_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASKS_H_

#include "base/pending_task.h"

namespace base {
namespace sequence_manager {

constexpr int kTaskTypeNone = 0;

// TODO(kraynov): Move TaskQueue::Task here.

namespace internal {

// Wrapper around PostTask method arguments and the assigned task type.
// Eventually it becomes a PendingTask once accepted by a TaskQueueImpl.
struct BASE_EXPORT PostedTask {
  explicit PostedTask(OnceClosure callback = OnceClosure(),
                      Location location = Location(),
                      TimeDelta delay = TimeDelta(),
                      Nestable nestable = Nestable::kNestable,
                      int task_type = kTaskTypeNone);
  PostedTask(PostedTask&& move_from) noexcept;
  ~PostedTask();

  OnceClosure callback;
  Location location;
  TimeDelta delay;
  Nestable nestable;
  int task_type;

  DISALLOW_COPY_AND_ASSIGN(PostedTask);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASKS_H_
