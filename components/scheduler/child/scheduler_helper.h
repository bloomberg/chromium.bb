// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
#define COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_

#include "components/scheduler/child/prioritizing_task_queue_selector.h"
#include "components/scheduler/child/task_queue_manager.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
class TickClock;
}

namespace scheduler {

class NestableSingleThreadTaskRunner;

// Common scheduler functionality for default tasks.
class SCHEDULER_EXPORT SchedulerHelper {
 public:
  // NOTE |total_task_queue_count| must be >= TASK_QUEUE_COUNT.
  // Category strings must have application lifetime (statics or
  // literals). They may not include " chars.
  SchedulerHelper(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
      const char* tracing_category,
      const char* disabled_by_default_tracing_category,
      const char* disabled_by_default_verbose_tracing_category,
      size_t total_task_queue_count);
  ~SchedulerHelper();

  // Returns the default task runner.
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner();

  // Returns the control task runner.  Tasks posted to this runner are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner();

  // Returns the control task after wakeup runner.  Tasks posted to this runner
  // are executed with the highest priority but do not cause the scheduler to
  // wake up. Care must be taken to avoid starvation of other task queues.
  scoped_refptr<base::SingleThreadTaskRunner> ControlAfterWakeUpTaskRunner();

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

  // Keep SchedulerHelper::TaskQueueIdToString in sync with this enum.
  enum QueueId {
    DEFAULT_TASK_QUEUE,
    CONTROL_TASK_QUEUE,
    CONTROL_TASK_AFTER_WAKEUP_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
    FIRST_QUEUE_ID = DEFAULT_TASK_QUEUE,
  };

  static const char* TaskQueueIdToString(QueueId queue_id);

  void CheckOnValidThread() const {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Accessor methods.
  base::TimeTicks Now() const;
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForQueue(
      size_t queue_index) const;
  base::TimeTicks NextPendingDelayedTaskRunTime() const;
  void SetQueueName(size_t queue_index, const char* name);
  bool IsQueueEmpty(size_t queue_index) const;
  TaskQueueManager::QueueState GetQueueState(size_t queue_index) const;
  void SetQueuePriority(size_t queue_index,
                        PrioritizingTaskQueueSelector::QueuePriority priority);
  void EnableQueue(size_t queue_index,
                   PrioritizingTaskQueueSelector::QueuePriority priority);
  void DisableQueue(size_t queue_index);
  bool IsQueueEnabled(size_t queue_index) const;
  void SetPumpPolicy(size_t queue_index,
                     TaskQueueManager::PumpPolicy pump_policy);
  void SetWakeupPolicy(size_t queue_index,
                       TaskQueueManager::WakeupPolicy wakeup_policy);
  void PumpQueue(size_t queue_index);
  uint64 GetQuiescenceMonitoredTaskQueueMask() const;
  uint64 GetAndClearTaskWasRunOnQueueBitmap();

  // Test helpers.
  void SetTimeSourceForTesting(scoped_ptr<base::TickClock> time_source);
  void SetWorkBatchSizeForTesting(size_t work_batch_size);
  TaskQueueManager* GetTaskQueueManagerForTesting();

 private:
  friend class SchedulerHelperTest;

  base::ThreadChecker thread_checker_;
  scoped_ptr<PrioritizingTaskQueueSelector> task_queue_selector_;
  scoped_ptr<TaskQueueManager> task_queue_manager_;

  uint64 quiescence_monitored_task_queue_mask_;

  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> control_after_wakeup_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  scoped_ptr<base::TickClock> time_source_;

  const char* tracing_category_;
  const char* disabled_by_default_tracing_category_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelper);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
