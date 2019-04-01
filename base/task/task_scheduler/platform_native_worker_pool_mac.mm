// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/platform_native_worker_pool_mac.h"

#include "base/task/task_scheduler/task_tracker.h"

namespace base {
namespace internal {

PlatformNativeWorkerPoolMac::PlatformNativeWorkerPoolMac(
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate,
    SchedulerWorkerPool* predecessor_pool)
    : PlatformNativeWorkerPool(std::move(task_tracker),
                               std::move(delegate),
                               predecessor_pool) {}

PlatformNativeWorkerPoolMac::~PlatformNativeWorkerPoolMac() {}

void PlatformNativeWorkerPoolMac::StartImpl() {
  queue_.reset(dispatch_queue_create(
      "org.chromium.base.TaskScheduler.WorkerPool", DISPATCH_QUEUE_CONCURRENT));
  group_.reset(dispatch_group_create());
}

void PlatformNativeWorkerPoolMac::JoinImpl() {
  dispatch_group_wait(group_, DISPATCH_TIME_FOREVER);
}

void PlatformNativeWorkerPoolMac::SubmitWork() {
  // TODO(adityakeerthi): Handle priorities by having multiple dispatch queues
  // with different qualities-of-service.
  dispatch_group_async(group_, queue_, ^{
    RunNextSequenceImpl();
  });
}

}  // namespace internal
}  // namespace base
