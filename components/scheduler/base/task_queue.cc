// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue.h"

namespace scheduler {

bool TaskQueue::HasPendingImmediateTask() const {
  QueueState state = GetQueueState();
  return state == QueueState::NEEDS_PUMPING || state == QueueState::HAS_WORK;
}

}  // namespace scheduler
