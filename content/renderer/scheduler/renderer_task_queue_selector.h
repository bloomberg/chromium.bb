// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_RENDERER_TASK_QUEUE_SELECTOR_H_
#define CONTENT_RENDERER_SCHEDULER_RENDERER_TASK_QUEUE_SELECTOR_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/scheduler/task_queue_selector.h"

namespace content {

// A RendererTaskQueueSelector is a TaskQueueSelector which is used by the
// RendererScheduler to enable prioritization of particular task queues.
class CONTENT_EXPORT RendererTaskQueueSelector
    : NON_EXPORTED_BASE(public TaskQueueSelector) {
 public:
  enum QueuePriority {
    // Queues with control priority will run before any other queue, and will
    // explicitly starve other queues. Typically this should only be used for
    // private queues which perform control operations.
    CONTROL_PRIORITY,
    // Queues with high priority will be selected preferentially over normal or
    // best effort queues. The selector will ensure that high priority queues
    // cannot completely starve normal priority queues.
    HIGH_PRIORITY,
    // Queues with normal priority are the default.
    NORMAL_PRIORITY,
    // Queues with best effort priority will only be run if all other queues are
    // empty. They can be starved by the other queues.
    BEST_EFFORT_PRIORITY,
    // Must be the last entry.
    QUEUE_PRIORITY_COUNT,
    FIRST_QUEUE_PRIORITY = CONTROL_PRIORITY,
  };

  RendererTaskQueueSelector();
  ~RendererTaskQueueSelector() override;

  // Set the priority of |queue_index| to |priority|.
  void SetQueuePriority(size_t queue_index, QueuePriority priority);

  // Enable the |queue_index| with a priority of |priority|. By default all
  // queues are enabled with normal priority.
  void EnableQueue(size_t queue_index, QueuePriority priority);

  // Disable the |queue_index|.
  void DisableQueue(size_t queue_index);

  // Whether |queue_index| is enabled.
  bool IsQueueEnabled(size_t queue_index) const;

  // TaskQueueSelector implementation:
  void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) override;
  bool SelectWorkQueueToService(size_t* out_queue_index) override;
  void AsValueInto(base::trace_event::TracedValue* state) const override;

 private:
  // Returns true if queueA contains an older task than queueB.
  static bool IsOlder(const base::TaskQueue* queueA,
                      const base::TaskQueue* queueB);

  // Returns the priority which is next after |priority|.
  static QueuePriority NextPriority(QueuePriority priority);

  static const char* PriorityToString(QueuePriority priority);

  // Return true if |out_queue_index| indicates the index of the queue with
  // the oldest pending task from the set of queues of |priority|, or
  // false if all queues of that priority are empty.
  bool ChooseOldestWithPriority(QueuePriority priority,
                                size_t* out_queue_index) const;

  // Returns true if |queue_index| is enabled with the given |priority|.
  bool QueueEnabledWithPriority(size_t queue_index,
                                QueuePriority priority) const;

  // Called whenever the selector chooses a task queue for execution with the
  // priority |priority|.
  void DidSelectQueueWithPriority(QueuePriority priority);

  // Number of high priority tasks which can be run before a normal priority
  // task should be selected to prevent starvation.
  // TODO(rmcilroy): Check if this is a good value.
  static const size_t kMaxStarvationTasks = 5;

  base::ThreadChecker main_thread_checker_;
  std::vector<const base::TaskQueue*> work_queues_;
  std::set<size_t> queue_priorities_[QUEUE_PRIORITY_COUNT];
  size_t starvation_count_;
  DISALLOW_COPY_AND_ASSIGN(RendererTaskQueueSelector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_RENDERER_TASK_QUEUE_SELECTOR_H_
