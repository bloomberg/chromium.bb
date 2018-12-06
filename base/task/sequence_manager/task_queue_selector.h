// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/pending_task.h"
#include "base/task/sequence_manager/task_queue_selector_logic.h"
#include "base/task/sequence_manager/work_queue_sets.h"

namespace base {
namespace sequence_manager {
namespace internal {

class AssociatedThreadId;

// TaskQueueSelector is used by the SchedulerHelper to enable prioritization
// of particular task queues.
class BASE_EXPORT TaskQueueSelector {
 public:
  explicit TaskQueueSelector(
      scoped_refptr<AssociatedThreadId> associated_thread);
  ~TaskQueueSelector();

  // Called to register a queue that can be selected. This function is called
  // on the main thread.
  void AddQueue(internal::TaskQueueImpl* queue);

  // The specified work will no longer be considered for selection. This
  // function is called on the main thread.
  void RemoveQueue(internal::TaskQueueImpl* queue);

  // Make |queue| eligible for selection. This function is called on the main
  // thread. Must only be called if |queue| is disabled.
  void EnableQueue(internal::TaskQueueImpl* queue);

  // Disable selection from |queue|. Must only be called if |queue| is enabled.
  void DisableQueue(internal::TaskQueueImpl* queue);

  // Called get or set the priority of |queue|.
  void SetQueuePriority(internal::TaskQueueImpl* queue,
                        TaskQueue::QueuePriority priority);

  // Called to choose the work queue from which the next task should be taken
  // and run. Return the queue to service if there is one or null otherwise.
  // This function is called on the main thread.
  WorkQueue* SelectWorkQueueToService();

  // Serialize the selector state for tracing.
  void AsValueInto(trace_event::TracedValue* state) const;

  class BASE_EXPORT Observer {
   public:
    virtual ~Observer() = default;

    // Called when |queue| transitions from disabled to enabled.
    virtual void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) = 0;
  };

  // Called once to set the Observer. This function is called
  // on the main thread. If |observer| is null, then no callbacks will occur.
  void SetTaskQueueSelectorObserver(Observer* observer);

  // Returns true if all the enabled work queues are empty. Returns false
  // otherwise.
  bool AllEnabledWorkQueuesAreEmpty() const;

 protected:
  WorkQueue* ChooseOldestWithPriority(TaskQueue::QueuePriority priority) const;

  WorkQueueSets* delayed_work_queue_sets() { return &delayed_work_queue_sets_; }

  WorkQueueSets* immediate_work_queue_sets() {
    return &immediate_work_queue_sets_;
  }

  // Maximum score to accumulate before high priority tasks are run even in
  // the presence of highest priority tasks.
  static const size_t kMaxHighPriorityStarvationScore = 3;

  // Increment to be applied to the high priority starvation score when a task
  // should have only a small effect on the score. E.g. A number of highest
  // priority tasks must run before the high priority queue is considered
  // starved.
  static const size_t kSmallScoreIncrementForHighPriorityStarvation = 1;

  // Maximum score to accumulate before normal priority tasks are run even in
  // the presence of higher priority tasks i.e. highest and high priority
  // tasks.
  static const size_t kMaxNormalPriorityStarvationScore = 5;

  // Increment to be applied to the normal priority starvation score when a
  // task should have a large effect on the score. E.g Only a few high
  // priority priority tasks must run before the normal priority queue is
  // considered starved.
  static const size_t kLargeScoreIncrementForNormalPriorityStarvation = 2;

  // Increment to be applied to the normal priority starvation score when a
  // task should have only a small effect on the score. E.g. A number of
  // highest priority tasks must run before the normal priority queue is
  // considered starved.
  static const size_t kSmallScoreIncrementForNormalPriorityStarvation = 1;

  // Maximum score to accumulate before low priority tasks are run even in the
  // presence of highest, high, or normal priority tasks.
  static const size_t kMaxLowPriorityStarvationScore = 25;

  // Increment to be applied to the low priority starvation score when a task
  // should have a large effect on the score. E.g. Only a few normal/high
  // priority tasks must run before the low priority queue is considered
  // starved.
  static const size_t kLargeScoreIncrementForLowPriorityStarvation = 5;

  // Increment to be applied to the low priority starvation score when a task
  // should have only a small effect on the score. E.g. A lot of highest
  // priority tasks must run before the low priority queue is considered
  // starved.
  static const size_t kSmallScoreIncrementForLowPriorityStarvation = 1;

 private:
  void ChangeSetIndex(internal::TaskQueueImpl* queue,
                      TaskQueue::QueuePriority priority);
  void AddQueueImpl(internal::TaskQueueImpl* queue,
                    TaskQueue::QueuePriority priority);
  void RemoveQueueImpl(internal::TaskQueueImpl* queue);

  WorkQueue* SelectWorkQueueToServiceImpl(
      TaskQueue::QueuePriority max_priority);

#if DCHECK_IS_ON() || !defined(NDEBUG)
  bool CheckContainsQueueForTest(const internal::TaskQueueImpl* queue) const;
#endif

  WorkQueue* ChooseOldestImmediateTaskWithPriority(
      TaskQueue::QueuePriority priority) const;

  WorkQueue* ChooseOldestDelayedTaskWithPriority(
      TaskQueue::QueuePriority priority) const;

  WorkQueue* ChooseOldestImmediateOrDelayedTaskWithPriority(
      TaskQueue::QueuePriority priority) const;

  // Returns the priority which is next after |priority|.
  static TaskQueue::QueuePriority NextPriority(
      TaskQueue::QueuePriority priority);

  bool SelectWorkQueueToServiceInternal(WorkQueue** out_work_queue);

  // Called whenever the selector chooses a task queue for execution with the
  // priority |priority|.
  void DidSelectQueueWithPriority(TaskQueue::QueuePriority priority,
                                  bool chose_delayed_queue);

  // Returns true if there are pending tasks with priority |priority|.
  bool HasTasksWithPriority(TaskQueue::QueuePriority priority);

  scoped_refptr<AssociatedThreadId> associated_thread_;

  WorkQueueSets delayed_work_queue_sets_;
  WorkQueueSets immediate_work_queue_sets_;
  bool choose_immediate_over_delayed_ = false;
  size_t high_priority_starvation_score_ = 0;
  size_t normal_priority_starvation_score_ = 0;
  size_t low_priority_starvation_score_ = 0;

  Observer* task_queue_selector_observer_ = nullptr;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(TaskQueueSelector);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_
