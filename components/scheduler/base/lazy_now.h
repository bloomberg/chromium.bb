// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_LAZY_NOW_H_
#define COMPONENTS_SCHEDULER_BASE_LAZY_NOW_H_

#include "base/time/time.h"

namespace scheduler {
class TaskQueueManager;

namespace internal {

// Now() is somewhat expensive so it makes sense not to call Now() unless we
// really need to.
class LazyNow {
 public:
  explicit LazyNow(base::TimeTicks now)
      : task_queue_manager_(nullptr), now_(now) {
    DCHECK(!now.is_null());
  }

  explicit LazyNow(TaskQueueManager* task_queue_manager)
      : task_queue_manager_(task_queue_manager) {}

  base::TimeTicks Now();

 private:
  TaskQueueManager* task_queue_manager_;  // NOT OWNED
  base::TimeTicks now_;
};

}  // namespace internal
}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_LAZY_NOW_H_
