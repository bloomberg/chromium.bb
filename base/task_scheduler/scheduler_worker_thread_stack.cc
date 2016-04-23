// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_thread_stack.h"

#include <algorithm>

#include "base/logging.h"

namespace base {
namespace internal {

SchedulerWorkerThreadStack::SchedulerWorkerThreadStack() = default;

SchedulerWorkerThreadStack::~SchedulerWorkerThreadStack() = default;

void SchedulerWorkerThreadStack::Push(SchedulerWorkerThread* worker_thread) {
  DCHECK(std::find(stack_.begin(), stack_.end(), worker_thread) == stack_.end())
      << "SchedulerWorkerThread already on stack";
  stack_.push_back(worker_thread);
}

SchedulerWorkerThread* SchedulerWorkerThreadStack::Pop() {
  if (IsEmpty())
    return nullptr;
  SchedulerWorkerThread* const worker_thread = stack_.back();
  stack_.pop_back();
  return worker_thread;
}

void SchedulerWorkerThreadStack::Remove(
    const SchedulerWorkerThread* worker_thread) {
  auto it = std::find(stack_.begin(), stack_.end(), worker_thread);
  if (it != stack_.end())
    stack_.erase(it);
}

}  // namespace internal
}  // namespace base
