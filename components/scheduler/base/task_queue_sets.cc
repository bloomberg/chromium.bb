// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_sets.h"

#include "base/logging.h"
#include "components/scheduler/base/task_queue_impl.h"

namespace scheduler {
namespace internal {

TaskQueueSets::TaskQueueSets(size_t num_sets)
    : enqueue_order_to_queue_maps_(num_sets) {}

TaskQueueSets::~TaskQueueSets() {}

void TaskQueueSets::RemoveQueue(internal::TaskQueueImpl* queue) {
  int enqueue_order;
  bool has_enqueue_order =
      queue->GetWorkQueueFrontTaskEnqueueOrder(&enqueue_order);
  if (!has_enqueue_order)
    return;
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size());
  DCHECK_EQ(
      queue,
      enqueue_order_to_queue_maps_[set_index].find(enqueue_order)->second);
  enqueue_order_to_queue_maps_[set_index].erase(enqueue_order);
}

void TaskQueueSets::AssignQueueToSet(internal::TaskQueueImpl* queue,
                                     size_t set_index) {
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size());
  int enqueue_order;
  bool has_enqueue_order =
      queue->GetWorkQueueFrontTaskEnqueueOrder(&enqueue_order);
  size_t old_set = queue->get_task_queue_set_index();
  DCHECK_LT(old_set, enqueue_order_to_queue_maps_.size());
  queue->set_task_queue_set_index(set_index);
  if (!has_enqueue_order)
    return;
  enqueue_order_to_queue_maps_[old_set].erase(enqueue_order);
  enqueue_order_to_queue_maps_[set_index].insert(
      std::make_pair(enqueue_order, queue));
}

void TaskQueueSets::OnPushQueue(internal::TaskQueueImpl* queue) {
  int enqueue_order;
  bool has_enqueue_order =
      queue->GetWorkQueueFrontTaskEnqueueOrder(&enqueue_order);
  DCHECK(has_enqueue_order);
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size()) << " set_index = "
                                                            << set_index;
  enqueue_order_to_queue_maps_[set_index].insert(
      std::make_pair(enqueue_order, queue));
}

void TaskQueueSets::OnPopQueue(internal::TaskQueueImpl* queue) {
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size());
  DCHECK(!enqueue_order_to_queue_maps_[set_index].empty()) << " set_index = "
                                                           << set_index;
  DCHECK_EQ(enqueue_order_to_queue_maps_[set_index].begin()->second, queue)
      << " set_index = " << set_index;
  // O(1) amortised.
  enqueue_order_to_queue_maps_[set_index].erase(
      enqueue_order_to_queue_maps_[set_index].begin());
  int enqueue_order;
  bool has_enqueue_order =
      queue->GetWorkQueueFrontTaskEnqueueOrder(&enqueue_order);
  if (!has_enqueue_order)
    return;
  enqueue_order_to_queue_maps_[set_index].insert(
      std::make_pair(enqueue_order, queue));
}

bool TaskQueueSets::GetOldestQueueInSet(
    size_t set_index,
    internal::TaskQueueImpl** out_queue) const {
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size());
  if (enqueue_order_to_queue_maps_[set_index].empty())
    return false;
  *out_queue = enqueue_order_to_queue_maps_[set_index].begin()->second;
  return true;
}

bool TaskQueueSets::IsSetEmpty(size_t set_index) const {
  DCHECK_LT(set_index, enqueue_order_to_queue_maps_.size()) << " set_index = "
                                                            << set_index;
  return enqueue_order_to_queue_maps_[set_index].empty();
}

}  // namespace internal
}  // namespace scheduler
