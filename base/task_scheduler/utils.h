// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_UTILS_H_
#define BASE_TASK_SCHEDULER_UTILS_H_

#include <memory>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"

namespace base {
namespace internal {

class DelayedTaskManager;
class PriorityQueue;
class SchedulerTaskExecutor;
class TaskTracker;

// Attempts to post |task| to the provided |sequence| and |executor| conditional
// on |task_tracker|. If |task| has a delayed run time, it is handled by
// |delayed_task_manager|. Returns true if the task is posted.
bool BASE_EXPORT PostTaskToExecutor(std::unique_ptr<Task> task,
                                    scoped_refptr<Sequence> sequence,
                                    SchedulerTaskExecutor* executor,
                                    TaskTracker* task_tracker,
                                    DelayedTaskManager* delayed_task_manager);

// Posts |task| to the provided |sequence| and |priority_queue|. This must only
// be called after |task|'s delayed run time. Returns true if |sequence| was
// empty before |task| was inserted into it.
bool BASE_EXPORT
AddTaskToSequenceAndPriorityQueue(std::unique_ptr<Task> task,
                                  scoped_refptr<Sequence> sequence,
                                  PriorityQueue* priority_queue);

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_UTILS_H_
