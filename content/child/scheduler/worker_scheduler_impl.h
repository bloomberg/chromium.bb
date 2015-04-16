// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_IMPL_H_
#define CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_IMPL_H_

#include "content/child/scheduler/worker_scheduler.h"
#include "content/child/scheduler/scheduler_helper.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace content {

class NestableSingleThreadTaskRunner;

class CONTENT_EXPORT WorkerSchedulerImpl
    : public WorkerScheduler,
      public SchedulerHelper::SchedulerHelperDelegate {
 public:
  explicit WorkerSchedulerImpl(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner);
  ~WorkerSchedulerImpl() override;

  // WorkerScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Init() override;
  void Shutdown() override;

  void SetTimeSourceForTesting(scoped_refptr<cc::TestNowSource> time_source);
  void SetWorkBatchSizeForTesting(size_t work_batch_size);
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

 protected:
  // SchedulerHelperDelegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}

 private:
  void MaybeStartLongIdlePeriod();

  SchedulerHelper helper_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_IMPL_H_
