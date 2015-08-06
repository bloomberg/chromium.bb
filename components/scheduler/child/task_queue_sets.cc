// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue_sets.h"

#include "base/logging.h"
#include "components/scheduler/child/task_queue_impl.h"

namespace scheduler {
namespace internal {

TaskQueueSets::TaskQueueSets(size_t num_sets) : age_to_queue_maps_(num_sets) {}

TaskQueueSets::~TaskQueueSets() {}

void TaskQueueSets::RemoveQueue(internal::TaskQueueImpl* queue) {
  int age;
  bool has_age = queue->GetWorkQueueFrontTaskAge(&age);
  if (!has_age)
    return;
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, age_to_queue_maps_.size());
  age_to_queue_maps_[set_index].erase(age);
}

void TaskQueueSets::AssignQueueToSet(internal::TaskQueueImpl* queue,
                                     size_t set_index) {
  DCHECK_LT(set_index, age_to_queue_maps_.size());
  int age;
  bool has_age = queue->GetWorkQueueFrontTaskAge(&age);
  size_t old_set = queue->get_task_queue_set_index();
  DCHECK_LT(old_set, age_to_queue_maps_.size());
  queue->set_task_queue_set_index(set_index);
  if (!has_age)
    return;
  age_to_queue_maps_[old_set].erase(age);
  age_to_queue_maps_[set_index].insert(std::make_pair(age, queue));
}

void TaskQueueSets::OnPushQueue(internal::TaskQueueImpl* queue) {
  int age;
  bool has_age = queue->GetWorkQueueFrontTaskAge(&age);
  DCHECK(has_age);
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, age_to_queue_maps_.size()) << " set_index = "
                                                  << set_index;
  age_to_queue_maps_[set_index].insert(std::make_pair(age, queue));
}

void TaskQueueSets::OnPopQueue(internal::TaskQueueImpl* queue) {
  size_t set_index = queue->get_task_queue_set_index();
  DCHECK_LT(set_index, age_to_queue_maps_.size());
  DCHECK(!age_to_queue_maps_[set_index].empty()) << " set_index = "
                                                 << set_index;
  DCHECK_EQ(age_to_queue_maps_[set_index].begin()->second, queue)
      << " set_index = " << set_index;
  // O(1) amortised.
  age_to_queue_maps_[set_index].erase(age_to_queue_maps_[set_index].begin());
  int age;
  bool has_age = queue->GetWorkQueueFrontTaskAge(&age);
  if (!has_age)
    return;
  age_to_queue_maps_[set_index].insert(std::make_pair(age, queue));
}

bool TaskQueueSets::GetOldestQueueInSet(
    size_t set_index,
    internal::TaskQueueImpl** out_queue) const {
  DCHECK_LT(set_index, age_to_queue_maps_.size());
  if (age_to_queue_maps_[set_index].empty())
    return false;
  *out_queue = age_to_queue_maps_[set_index].begin()->second;
  return true;
}

}  // namespace internal
}  // namespace scheduler
