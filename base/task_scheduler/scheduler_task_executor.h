// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_TASK_EXECUTOR_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_TASK_EXECUTOR_H_

#include <memory>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"

namespace base {
namespace internal {

// Interface for a component that runs Tasks.
class BASE_EXPORT SchedulerTaskExecutor {
 public:
  virtual ~SchedulerTaskExecutor() = default;

  // Posts |task| to be executed by this component as part of |sequence|. The
  // scheduler's TaskTracker must have allowed |task| to be posted before this
  // is called. This must only be called after |task|'s delayed run time.
  virtual void PostTaskWithSequence(std::unique_ptr<Task> task,
                                    scoped_refptr<Sequence> sequence) = 0;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_TASK_EXECUTOR_H_
