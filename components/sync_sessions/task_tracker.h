// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TASK_TRACKER_H_
#define COMPONENTS_SYNC_SESSIONS_TASK_TRACKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "ui/base/page_transition_types.h"

namespace sync_sessions {

// Class to generate and manage task ids for navigations of a tab. For each
// current navigation of a tab, UpdateWithNavigation(int navigation_index,
//            ui::PageTransition transition)
// needs to be called to update the object.
//
// TODO(shenchao): If the tab is restored, then the input navigation is not
// necessarily the first navigation in this case. Need to fix it by initalizing
// the object with restored data.
// TODO(shenchao): Support to track tasks cross tabs.
class TabTasks {
 public:
  TabTasks();
  virtual ~TabTasks();

  // Gets top-down task id list of ancestors and itself for
  // |navigation_index|-th navigation of the tab.
  std::vector<int64_t> GetTaskIdsForNavigation(int navigation_index) const;

  int GetNavigationsCount() const;

  // Updates the current task of the tab, given current navigation index of the
  // tab as |navigation_index|, and its |transition|.
  // If the navigation is from going back/forward of the tab, we set its first
  // visit as current task; if the navigation is new, we create a subtask of the
  // previous navigation if it's linked from the previous one or a root task
  // otherwise, and use |navigation_id| as new task id.
  void UpdateWithNavigation(int navigation_index,
                            ui::PageTransition transition,
                            int64_t navigation_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskTrackerTest, LimitMaxNumberOfTasksPerTab);

  FRIEND_TEST_ALL_PREFIXES(TaskTrackerTest,
                           CreateSubTaskFromExcludedAncestorTask);

  struct TaskIdAndRoot {
    // Root task index in task_ids_. Negative value means it's an invalid task
    // just for filling the task_ids_.
    int root_navigation_index;
    int64_t task_id;
  };

  // Task ids (with root task) for the navigations of the tab. The vector is
  // corresponding to the sequence of navigations of the tab.
  std::vector<TaskIdAndRoot> task_ids_;
  // Index of current navigation in task_ids_.
  int current_navigation_index_ = -1;
  // Number of oldest ancestors which have been excluded from being tracked in
  // task_ids_;
  int excluded_navigation_num_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TabTasks);
};

// Tracks tasks of current session.
class TaskTracker {
 public:
  // Constructs with a clock to get timestamp as new task ids.
  TaskTracker();
  virtual ~TaskTracker();

  // Returns a TabTasks pointer, which is owned by this object, for the tab of
  // given |tab_id|.
  TabTasks* GetTabTasks(SessionID::id_type tab_id);

  // Cleans tracked task ids of navigations in the tab of |tab_id|.
  void CleanTabTasks(SessionID::id_type tab_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskTrackerTest, CleanTabTasks);
  std::map<SessionID::id_type, std::unique_ptr<TabTasks>> local_tab_tasks_map_;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TASK_TRACKER_H_
