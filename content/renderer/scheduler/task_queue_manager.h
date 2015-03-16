// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_
#define CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_

#include "base/atomic_sequence_num.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class TestNowSource;
}

namespace content {
namespace internal {
class TaskQueue;
}
class TaskQueueSelector;
class NestableSingleThreadTaskRunner;

// The task queue manager provides N task queues and a selector interface for
// choosing which task queue to service next. Each task queue consists of two
// sub queues:
//
// 1. Incoming task queue. Tasks that are posted get immediately appended here.
//    When a task is appended into an empty incoming queue, the task manager
//    work function (DoWork) is scheduled to run on the main task runner.
//
// 2. Work queue. If a work queue is empty when DoWork() is entered, tasks from
//    the incoming task queue (if any) are moved here. The work queues are
//    registered with the selector as input to the scheduling decision.
//
class CONTENT_EXPORT TaskQueueManager {
 public:
  // Keep TaskQueue::PumpPolicyToString in sync with this enum.
  enum class PumpPolicy {
    // Tasks posted to an incoming queue with an AUTO pump policy will be
    // automatically scheduled for execution or transferred to the work queue
    // automatically.
    AUTO,
    // Tasks posted to an incoming queue with an AFTER_WAKEUP pump policy
    // will be scheduled for execution or transferred to the work queue
    // automatically but only after another queue has executed a task.
    AFTER_WAKEUP,
    // Tasks posted to an incoming queue with a MANUAL will not be
    // automatically scheduled for execution or transferred to the work queue.
    // Instead, the selector should call PumpQueue() when necessary to bring
    // in new tasks for execution.
    MANUAL
  };

  // Create a task queue manager with |task_queue_count| task queues.
  // |main_task_runner| identifies the thread on which where the tasks are
  // eventually run. |selector| is used to choose which task queue to service.
  // It should outlive this class.
  TaskQueueManager(
      size_t task_queue_count,
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
      TaskQueueSelector* selector);
  ~TaskQueueManager();

  // Returns the task runner which targets the queue selected by |queue_index|.
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForQueue(
      size_t queue_index) const;

  // Sets the pump policy for the |queue_index| to |pump_policy|. By
  // default queues are created with AUTO_PUMP_POLICY.
  void SetPumpPolicy(size_t queue_index, PumpPolicy pump_policy);

  // Reloads new tasks from the incoming queue for |queue_index| into the work
  // queue, regardless of whether the work queue is empty or not. After this,
  // this function ensures that the tasks in the work queue, if any, are
  // scheduled for execution.
  //
  // This function only needs to be called if automatic pumping is disabled
  // for |queue_index|. See |SetQueueAutoPumpPolicy|. By default automatic
  // pumping is enabled for all queues.
  void PumpQueue(size_t queue_index);

  // Returns true if there no tasks in either the work or incoming task queue
  // identified by |queue_index|. Note that this function involves taking a
  // lock, so calling it has some overhead.
  bool IsQueueEmpty(size_t queue_index) const;

  // Returns the time of the next pending delayed task in any queue. Returns
  // a null TimeTicks object if no tasks are pending.
  base::TimeTicks NextPendingDelayedTaskRunTime();

  // Set the name |queue_index| for tracing purposes. |name| must be a pointer
  // to a static string.
  void SetQueueName(size_t queue_index, const char* name);

  // Set the number of tasks executed in a single invocation of the task queue
  // manager. Increasing the batch size can reduce the overhead of yielding
  // back to the main message loop -- at the cost of potentially delaying other
  // tasks posted to the main loop. The batch size is 1 by default.
  void SetWorkBatchSize(int work_batch_size);

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  void SetTimeSourceForTesting(scoped_refptr<cc::TestNowSource> time_source);

 private:
  friend class internal::TaskQueue;

  // Called by the task queue to register a new pending task and allocate a
  // sequence number for it.
  void DidQueueTask(base::PendingTask* pending_task);

  // Post a task to call DoWork() on the main task runner.  Only one pending
  // DoWork is allowed from the main thread, to prevent an explosion of pending
  // DoWorks.
  void MaybePostDoWorkOnMainRunner();

  // Use the selector to choose a pending task and run it.
  void DoWork(bool posted_from_main_thread);

  // Reloads any empty work queues which have automatic pumping enabled and
  // which are eligible to be auto pumped based on the |previous_task| which was
  // run. Call with an empty |previous_task| if no task was just run. Returns
  // true if any work queue has tasks after doing this.
  // |next_pending_delayed_task| should be the time of the next known delayed
  // task. It is updated if any task is found which should run earlier.
  bool UpdateWorkQueues(base::TimeTicks* next_pending_delayed_task,
                        const base::PendingTask* previous_task);

  // Chooses the next work queue to service. Returns true if |out_queue_index|
  // indicates the queue from which the next task should be run, false to
  // avoid running any tasks.
  bool SelectWorkQueueToService(size_t* out_queue_index);

  // Runs a single nestable task from the work queue designated by
  // |queue_index|. If |has_previous_task| is true, |previous_task| should
  // contain the previous task in this work batch. Non-nestable task are
  // reposted on the run loop. The queue must not be empty.
  void ProcessTaskFromWorkQueue(size_t queue_index,
                                bool has_previous_task,
                                base::PendingTask* previous_task);

  bool RunsTasksOnCurrentThread() const;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay);
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay);
  internal::TaskQueue* Queue(size_t queue_index) const;

  base::TimeTicks Now() const;

  scoped_refptr<base::trace_event::ConvertableToTraceFormat>
  AsValueWithSelectorResult(bool should_run, size_t selected_queue) const;

  std::vector<scoped_refptr<internal::TaskQueue>> queues_;
  base::AtomicSequenceNumber task_sequence_num_;
  base::debug::TaskAnnotator task_annotator_;

  base::ThreadChecker main_thread_checker_;
  scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner_;
  TaskQueueSelector* selector_;

  base::WeakPtr<TaskQueueManager> task_queue_manager_weak_ptr_;

  // The pending_dowork_count_ is only tracked on the main thread since that's
  // where re-entrant problems happen.
  int pending_dowork_count_;

  int work_batch_size_;

  scoped_refptr<cc::TestNowSource> time_source_;

  ObserverList<base::MessageLoop::TaskObserver> task_observers_;

  base::WeakPtrFactory<TaskQueueManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_
