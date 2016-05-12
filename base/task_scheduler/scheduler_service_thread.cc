// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_service_thread.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/scheduler_worker_thread.h"
#include "base/task_scheduler/sequence.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {
namespace internal {
namespace {

class ServiceThreadDelegate : public SchedulerWorkerThread::Delegate {
 public:
  ServiceThreadDelegate(DelayedTaskManager* delayed_task_manager)
      : delayed_task_manager_(delayed_task_manager) {}

  // SchedulerWorkerThread::Delegate:
  void OnMainEntry(SchedulerWorkerThread* worker_thread) override {}

  scoped_refptr<Sequence> GetWork(SchedulerWorkerThread* worker_thread)
      override {
    delayed_task_manager_->PostReadyTasks();
    return nullptr;
  }

  void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override {
    NOTREACHED() <<
        "GetWork() never returns a sequence so there's nothing to reenqueue.";
  }

  TimeDelta GetSleepTimeout() override {
    const TimeTicks next_time = delayed_task_manager_->GetDelayedRunTime();
    if (next_time.is_null())
      return TimeDelta::Max();

    // For delayed tasks with delays that are really close to each other, it is
    // possible for the current time to advance beyond the required
    // GetDelayedWaitTime. Return a minimum of TimeDelta() in the event that
    // happens.
    TimeDelta sleep_time = next_time - delayed_task_manager_->Now();
    const TimeDelta zero_delta;
    return sleep_time < zero_delta ? zero_delta : sleep_time;
  }

 private:
  DelayedTaskManager* const delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceThreadDelegate);
};

}  // namespace

SchedulerServiceThread::~SchedulerServiceThread() = default;

// static
std::unique_ptr<SchedulerServiceThread> SchedulerServiceThread::Create(
    TaskTracker* task_tracker, DelayedTaskManager* delayed_task_manager) {
  std::unique_ptr<SchedulerWorkerThread> worker_thread =
      SchedulerWorkerThread::Create(
          ThreadPriority::NORMAL,
          WrapUnique(new ServiceThreadDelegate(delayed_task_manager)),
          task_tracker);
  if (!worker_thread)
    return nullptr;

  return WrapUnique(new SchedulerServiceThread(std::move(worker_thread)));
}

void SchedulerServiceThread::WakeUp() {
  worker_thread_->WakeUp();
}

void SchedulerServiceThread::JoinForTesting() {
  worker_thread_->JoinForTesting();
}

SchedulerServiceThread::SchedulerServiceThread(
    std::unique_ptr<SchedulerWorkerThread> worker_thread)
    : worker_thread_(std::move(worker_thread)) {}

}  // namespace internal
}  // namespace base
