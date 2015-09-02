// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_OBSERVER_H_

#include <vector>

#include "base/time/time.h"

namespace task_management {

class TaskManagerInterface;

typedef int64 TaskId;
typedef std::vector<TaskId> TaskIdList;

// Defines a list of types of resources that an observer needs to be refreshed
// on every task manager refresh cycle.
enum RefreshType {
  REFRESH_TYPE_NONE              = 0,
  REFRESH_TYPE_CPU               = 1,
  REFRESH_TYPE_MEMORY            = 1 << 1,
  REFRESH_TYPE_GPU_MEMORY        = 1 << 2,
  REFRESH_TYPE_V8_MEMORY         = 1 << 3,
  REFRESH_TYPE_SQLITE_MEMORY     = 1 << 4,
  REFRESH_TYPE_WEBCACHE_STATS    = 1 << 5,
  REFRESH_TYPE_NETWORK_USAGE     = 1 << 6,
  REFRESH_TYPE_NACL              = 1 << 7,
  REFRESH_TYPE_IDLE_WAKEUPS      = 1 << 8,
  REFRESH_TYPE_HANDLES           = 1 << 9,
};

// Defines the interface for observers of the task manager.
class TaskManagerObserver {
 public:
  // Constructs a TaskManagerObserver given the minimum |refresh_time| that it
  // it requires the task manager to be refreshing the values at, along with the
  // |resources_flags| that it needs to be calculated on each refresh cycle of
  // the task manager (use the above flags in |ResourceType|).
  //
  // Notes:
  // 1- The task manager will refresh at least once every |refresh_time| as
  // long as this observer is added to it. There might be other observers that
  // require more frequent refreshes.
  // 2- Refresh time values less than 1 second will be considered as 1 second.
  // 3- Depending on the other observers, the task manager may refresh more
  // resources than those defined in |resources_flags|.
  // 4- Upon the removal of the observer from the task manager, the task manager
  // will update its refresh time and the calculated resources to be the minimum
  // required value of all the remaining observers.
  TaskManagerObserver(base::TimeDelta refresh_time, int64 resources_flags);
  virtual ~TaskManagerObserver();

  // Notifies the observer that a chrome task with |id| has started and the task
  // manager is now monitoring it. The resource usage of this newly-added task
  // will remain invalid until the next refresh cycle of the task manager.
  virtual void OnTaskAdded(TaskId id) = 0;

  // Notifies the observer that a chrome task with |id| is about to be destroyed
  // and removed from the task manager right after this call. Observers which
  // are interested in doing some calculations related to the resource usage of
  // this task upon its removal may do so inside this call.
  virtual void OnTaskToBeRemoved(TaskId id) = 0;

  // Notifies the observer that the task manager has just finished a refresh
  // cycle to calculate the resources usage of all tasks whose IDs are given in
  // |task_ids|. |task_ids| will be sorted such that the task representing the
  // browser process is at the top of the list and the rest of the IDs will be
  // sorted by the process IDs on which the tasks are running, then by the task
  // IDs themselves.
  virtual void OnTasksRefreshed(const TaskIdList& task_ids) = 0;

  const base::TimeDelta& desired_refresh_time() const {
    return desired_refresh_time_;
  }

  int64 desired_resources_flags() const { return desired_resources_flags_; }

 protected:
  TaskManagerInterface* observed_task_manager() const {
    return observed_task_manager_;
  }

  // Add or Remove a refresh |type|.
  void AddRefreshType(RefreshType type);
  void RemoveRefreshType(RefreshType type);

 private:
  friend class TaskManagerInterface;

  // The currently observed task Manager.
  TaskManagerInterface* observed_task_manager_;

  // The minimum update time of the task manager that this observer needs to
  // do its job.
  base::TimeDelta desired_refresh_time_;

  // The flags that contain the resources that this observer needs to be
  // calculated on each refresh.
  int64 desired_resources_flags_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerObserver);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_OBSERVER_H_
