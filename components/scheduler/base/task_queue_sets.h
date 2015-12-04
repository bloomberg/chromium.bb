// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SETS_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SETS_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
namespace internal {
class TaskQueueImpl;

class SCHEDULER_EXPORT TaskQueueSets {
 public:
  // TODO(alexclarke): Consider refactoring TaskQueueImpl to have two explicit
  // WorkQueue objects (supporting PushOntoImmediateIncomingQueueLocked) so we
  // don't have to pass this enum around.
  enum class TaskType { DELAYED, IMMEDIATE };

  TaskQueueSets(TaskType task_type, size_t num_sets);
  ~TaskQueueSets();

  // O(log num queues)
  void RemoveQueue(internal::TaskQueueImpl* queue);

  // O(log num queues)
  void MoveQueue(internal::TaskQueueImpl* queue,
                 size_t old_set_index,
                 size_t new_set_index);

  // O(log num queues)
  void OnPushQueue(internal::TaskQueueImpl* queue);

  // If empty it's O(1) amortized, otherwise it's O(log num queues)
  void OnPopQueue(internal::TaskQueueImpl* queue);

  // O(1)
  bool GetOldestQueueInSet(size_t set_index,
                           internal::TaskQueueImpl** out_queue) const;

  // O(1)
  bool IsSetEmpty(size_t set_index) const;

  TaskType task_type() const { return task_type_; }

  static const char* TaskTypeToString(TaskType task_type);

 private:
  bool GetWorkQueueFrontTaskEnqueueOrder(internal::TaskQueueImpl* queue,
                                         int* enqueue_order) const;

  struct EnqueueOrderComparitor {
    // The enqueueorder numbers are generated in sequence.  These will
    // eventually overflow and roll-over to negative numbers.  We must take care
    // to preserve the ordering of the map when this happens.
    // NOTE we assume that tasks don't get starved for extended periods so that
    // the task queue ages in a set have at most one roll-over.
    // NOTE signed integer overflow behavior is undefined in C++ so we can't
    // use the (a - b) < 0 trick here, because the optimizer won't necessarily
    // do what we expect.
    // TODO(alexclarke): Consider making age and sequence_num unsigned, because
    // unsigned integer overflow behavior is defined.
    bool operator()(int a, int b) const {
      if (a < 0 && b >= 0)
        return false;
      if (b < 0 && a >= 0)
        return true;
      return a < b;
    }
  };

  typedef std::map<int, internal::TaskQueueImpl*, EnqueueOrderComparitor>
      EnqueueOrderToQueueMap;
  std::vector<EnqueueOrderToQueueMap> enqueue_order_to_queue_maps_;

  const TaskType task_type_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueSets);
};

}  // namespace internal
}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SETS_H_
