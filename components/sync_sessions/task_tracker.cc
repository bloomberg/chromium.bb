// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/task_tracker.h"

#include <utility>

#include "base/numerics/safe_conversions.h"

namespace sync_sessions {

namespace {
// The maximum number of tasks we track in a tab.
int kMaxNumTasksPerTab = 100;
}

TabTasks::TabTasks() {}

TabTasks::~TabTasks() {}

std::vector<int64_t> TabTasks::GetTaskIdsForNavigation(
    int navigation_index) const {
  CHECK_LE(0, navigation_index);
  CHECK_LT(navigation_index, GetNavigationsCount());

  std::vector<int64_t> root_to_self_task_ids;
  // Position of the navigation in task_ids_ vector.
  int navigation_position = navigation_index - excluded_navigation_num_;

  // If navigation_index is an excluded ancestor task, returns empty.
  if (navigation_position < 0)
    return root_to_self_task_ids;

  TaskIdAndRoot task_id_and_root = task_ids_[navigation_position];

  // If navigation_index is an invalid task, returns empty.
  if (task_id_and_root.root_navigation_index < 0)
    return root_to_self_task_ids;

  // The root task can be excluded. If so, consider the oldest ancestor
  // available as root.
  int root_navigation_index =
      task_id_and_root.root_navigation_index > excluded_navigation_num_
          ? task_id_and_root.root_navigation_index - excluded_navigation_num_
          : 0;
  for (int i = root_navigation_index; i <= navigation_position; i++) {
    // Fills the vector with valid tasks.
    if (task_ids_[i].root_navigation_index >= 0)
      root_to_self_task_ids.push_back(task_ids_[i].task_id);
  }
  return root_to_self_task_ids;
}

int TabTasks::GetNavigationsCount() const {
  return excluded_navigation_num_ + task_ids_.size();
}

void TabTasks::UpdateWithNavigation(int navigation_index,
                                    ui::PageTransition transition,
                                    int64_t navigation_id) {
  // Triggered by some notifications on the current page, do nothing.
  if (navigation_index == current_navigation_index_) {
    DVLOG(1) << "Doing nothing for navigation_index: " << navigation_index
             << " of transition: " << transition;
    return;
  }

  // Going back/forward to some previous navigation.
  if (navigation_index < current_navigation_index_ ||
      (navigation_index > current_navigation_index_ &&
       transition & ui::PAGE_TRANSITION_FORWARD_BACK &&
       base::checked_cast<size_t>(navigation_index) < task_ids_.size())) {
    DVLOG(1) << "Just updating task position with navigation_index: "
             << navigation_index << " of transition: " << transition;
    current_navigation_index_ = navigation_index;
    return;
  }

  // A new task for the new navigation.
  int root_navigation_index = navigation_index;
  if (current_navigation_index_ != -1 &&
      (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
       ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_AUTO_SUBFRAME) ||
       ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_MANUAL_SUBFRAME) ||
       ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_FORM_SUBMIT) ||
       transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK)) {
    // Creating a sub-task with navigation at current_navigation_index as
    // parent.
    DVLOG(1) << "Creating a sub-task with navigation_index: "
             << navigation_index << " of transition: " << transition
             << " under navigation_index: " << current_navigation_index_;
    // Position in task_id_.
    int current_navigation_position =
        current_navigation_index_ - excluded_navigation_num_;
    // If current/parent task is excluded, consider the new task as a root task.
    if (current_navigation_position >= 0) {
      CHECK_LT(current_navigation_position,
               base::checked_cast<int>(task_ids_.size()));
      root_navigation_index =
          task_ids_[current_navigation_position].root_navigation_index;
    } else {
      DVLOG(1) << "Becaue parent task is excluded, consider the sub-task as a "
                  "root task.";
    }
  } else {
    // Creating a root task.
    // For now, we don't consider tasks cross tabs, so first navigation of the
    // tab always creates a root task.
    DVLOG(1) << "Creating a root task with navigation_index: "
             << navigation_index << " of transition: " << transition;
  }

  // In most cases navigation_index == excluded_navigation_num_ +
  // task_ids_.size() if the previous navigation is end of chain, or
  // navigation_index < excluded_navigation_num_ + task_ids_.size() otherwise.
  // In few case navigation_index > excluded_navigation_num_ + task_ids_.size(),
  // we fill task_ids_ with invalid contents. A known case is the first
  // navigation after newtab.
  for (int i = task_ids_.size() + excluded_navigation_num_;
       i < navigation_index; i++) {
    task_ids_.push_back({-1, -1});
  }

  // Erase all task ids associated with an outdated forward navigation stack.
  if (navigation_index > excluded_navigation_num_) {
    int new_task_id_position = navigation_index - excluded_navigation_num_;
    task_ids_.erase(task_ids_.begin() + new_task_id_position, task_ids_.end());
  } else {
    excluded_navigation_num_ = navigation_index;
    // new task id position is 0
    task_ids_.clear();
  }

  // Exclude oldest ancestors if task number reaches the limit.
  int more_tasks_number = task_ids_.size() + 1 - kMaxNumTasksPerTab;
  if (more_tasks_number > 0) {
    task_ids_.erase(task_ids_.begin(), task_ids_.begin() + more_tasks_number);
    DVLOG(1) << "Excluding " << more_tasks_number
             << " oldest ancestor(s) from navigation index "
             << excluded_navigation_num_;
    excluded_navigation_num_ += more_tasks_number;
  }

  TaskIdAndRoot new_task = {root_navigation_index, navigation_id};
  // Add the current task at navigation_index.
  task_ids_.push_back(new_task);
  current_navigation_index_ = navigation_index;
  return;
}

TaskTracker::TaskTracker() {}

TaskTracker::~TaskTracker() {}

TabTasks* TaskTracker::GetTabTasks(SessionID::id_type tab_id) {
  if (local_tab_tasks_map_.find(tab_id) == local_tab_tasks_map_.end()) {
    local_tab_tasks_map_[tab_id] = base::MakeUnique<TabTasks>();
  }
  return local_tab_tasks_map_[tab_id].get();
}

void TaskTracker::CleanTabTasks(SessionID::id_type tab_id) {
  auto iter = local_tab_tasks_map_.find(tab_id);
  if (iter != local_tab_tasks_map_.end()) {
    local_tab_tasks_map_.erase(iter);
  }
}

}  // namespace sync_sessions
