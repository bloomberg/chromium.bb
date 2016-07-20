// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_pool_params.h"

namespace base {
namespace internal {

SchedulerWorkerPoolParams::SchedulerWorkerPoolParams(
    const std::string& name,
    ThreadPriority thread_priority,
    IORestriction io_restriction,
    int max_threads)
    : name_(name),
      thread_priority_(thread_priority),
      io_restriction_(io_restriction),
      max_threads_(max_threads) {}

SchedulerWorkerPoolParams::SchedulerWorkerPoolParams(
    SchedulerWorkerPoolParams&& other) = default;

SchedulerWorkerPoolParams& SchedulerWorkerPoolParams::operator=(
    SchedulerWorkerPoolParams&& other) = default;

}  // namespace internal
}  // namespace base
