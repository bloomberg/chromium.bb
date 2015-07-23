// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
#define COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_

#include "components/scheduler/child/task_queue_manager.h"
#include "components/scheduler/child/task_queue_selector.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
class TickClock;
}

namespace scheduler {

class SchedulerTaskRunnerDelegate;

// Common scheduler functionality for default tasks.
class SCHEDULER_EXPORT SchedulerHelper {
 public:
  // Category strings must have application lifetime (statics or
  // literals). They may not include " chars.
  SchedulerHelper(scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner,
                  const char* tracing_category,
                  const char* disabled_by_default_tracing_category,
                  const char* disabled_by_default_verbose_tracing_category);
  ~SchedulerHelper();

  // Returns the default task runner.
  scoped_refptr<TaskQueue> DefaultTaskRunner();

  // Returns the control task runner.  Tasks posted to this runner are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  scoped_refptr<TaskQueue> ControlTaskRunner();

  // Returns the control task after wakeup runner.  Tasks posted to this runner
  // are executed with the highest priority but do not cause the scheduler to
  // wake up. Care must be taken to avoid starvation of other task queues.
  scoped_refptr<TaskQueue> ControlAfterWakeUpTaskRunner();

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this class was created on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task runners will be
  // silently dropped.
  void Shutdown();

  // Returns true if Shutdown() has been called. Otherwise returns false.
  bool IsShutdown() const { return !task_queue_manager_.get(); }

  void CheckOnValidThread() const {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Creates a new TaskQueue with the given |spec|.
  scoped_refptr<TaskQueue> NewTaskQueue(const TaskQueue::Spec& spec);

  // Accessor methods.
  base::TimeTicks Now() const;
  base::TimeTicks NextPendingDelayedTaskRunTime() const;
  bool GetAndClearSystemIsQuiescentBit();

  // Test helpers.
  void SetTimeSourceForTesting(scoped_ptr<base::TickClock> time_source);
  void SetWorkBatchSizeForTesting(size_t work_batch_size);
  TaskQueueManager* GetTaskQueueManagerForTesting();

 private:
  friend class SchedulerHelperTest;

  base::ThreadChecker thread_checker_;
  scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner_;
  scoped_ptr<TaskQueueManager> task_queue_manager_;
  scoped_refptr<TaskQueue> control_task_runner_;
  scoped_refptr<TaskQueue> control_after_wakeup_task_runner_;
  scoped_refptr<TaskQueue> default_task_runner_;

  scoped_ptr<base::TickClock> time_source_;

  const char* tracing_category_;
  const char* disabled_by_default_tracing_category_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelper);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
