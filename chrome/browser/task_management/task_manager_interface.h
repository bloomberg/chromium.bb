// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/task_management/task_manager_observer.h"

namespace task_management {

// Defines the interface for any implementation of the task manager.
// Concrete implementations have no control over the refresh rate nor the
// enabled calculations of the usage of the various resources.
class TaskManagerInterface {
 public:
  void AddObserver(TaskManagerObserver* observer);
  void RemoveObserver(TaskManagerObserver* observer);

  // Returns true if the resource |type| usage calculation is enabled and
  // the implementation should refresh its value (this means that at least one
  // of the observers require this value). False otherwise.
  bool IsResourceRefreshEnabled(RefreshType type);

 protected:
  TaskManagerInterface();
  virtual ~TaskManagerInterface();

  // Notifying observers of various events.
  void NotifyObserversOnTaskAdded(TaskId id);
  void NotifyObserversOnTaskToBeRemoved(TaskId id);
  void NotifyObserversOnRefresh(const TaskIdList& task_ids);

  // Refresh all the enabled resources usage of all the available tasks.
  virtual void Refresh() = 0;
  // TODO(afakhry): Add more virtual methods here as needed.

  // Returns the current refresh time that this task manager is running at. It
  // will return base::TimeDelta::Max() if the task manager is not running.
  base::TimeDelta GetCurrentRefreshTime() const;

  int64 enabled_resources_flags() const { return enabled_resources_flags_; }

  void set_timer_for_testing(scoped_ptr<base::Timer> timer) {
    refresh_timer_ = timer.Pass();
  }

 private:
  // Appends |flags| to the |enabled_resources_flags_|.
  void ResourceFlagsAdded(int64 flags);

  // Sets |enabled_resources_flags_| to |flags|.
  void SetEnabledResourceFlags(int64 flags);

  // Schedules the task manager refresh cycles using the given |refresh_time|.
  // It stops any existing refresh schedule.
  void ScheduleRefresh(base::TimeDelta refresh_time);

  // The list of observers.
  base::ObserverList<TaskManagerObserver> observers_;

  // The timer that will be used to schedule the successive refreshes.
  scoped_ptr<base::Timer> refresh_timer_;

  // The flags containing the enabled resources types calculations.
  int64 enabled_resources_flags_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerInterface);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_
