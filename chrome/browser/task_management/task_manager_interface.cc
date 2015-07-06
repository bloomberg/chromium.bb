// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_manager_interface.h"

namespace task_management {

void TaskManagerInterface::AddObserver(TaskManagerObserver* observer) {
  observers_.AddObserver(observer);

  ResourceFlagsAdded(observer->desired_resources_flags());

  if (observer->desired_refresh_time() > GetCurrentRefreshTime())
    return;

  // Reached here, then this is EITHER (not the first observer to be added AND
  // it requires a more frequent refresh rate) OR (it's the very first observer
  // to be added).
  // Reset the refresh timer.
  ScheduleRefresh(observer->desired_refresh_time());
}

void TaskManagerInterface::RemoveObserver(TaskManagerObserver* observer) {
  observers_.RemoveObserver(observer);

  // Recalculate the minimum refresh rate and the enabled resource flags.
  int64 flags = 0;
  base::TimeDelta min_time = base::TimeDelta::Max();
  base::ObserverList<TaskManagerObserver>::Iterator itr(&observers_);
  TaskManagerObserver* obs;
  while ((obs = itr.GetNext()) != nullptr) {
    if (obs->desired_refresh_time() < min_time)
      min_time = obs->desired_refresh_time();

    flags |= obs->desired_resources_flags();
  }

  if (min_time == base::TimeDelta::Max()) {
    SetEnabledResourceFlags(0);
    refresh_timer_->Stop();
  } else {
    SetEnabledResourceFlags(flags);
    ScheduleRefresh(min_time);
  }
}

bool TaskManagerInterface::IsResourceRefreshEnabled(RefreshType type) {
  return (enabled_resources_flags_ & type) != 0;
}

TaskManagerInterface::TaskManagerInterface()
    : observers_(),
      refresh_timer_(new base::Timer(true, true)),
      enabled_resources_flags_(0) {
}

TaskManagerInterface::~TaskManagerInterface() {
}

void TaskManagerInterface::NotifyObserversOnTaskAdded(TaskId id) {
  FOR_EACH_OBSERVER(TaskManagerObserver, observers_, OnTaskAdded(id));
}

void TaskManagerInterface::NotifyObserversOnTaskToBeRemoved(TaskId id) {
  FOR_EACH_OBSERVER(TaskManagerObserver, observers_, OnTaskToBeRemoved(id));
}

void TaskManagerInterface::NotifyObserversOnRefresh(
    const TaskIdList& task_ids) {
  FOR_EACH_OBSERVER(TaskManagerObserver,
                    observers_,
                    OnTasksRefreshed(task_ids));
}

base::TimeDelta TaskManagerInterface::GetCurrentRefreshTime() const {
  return refresh_timer_->IsRunning() ? refresh_timer_->GetCurrentDelay()
                                     : base::TimeDelta::Max();
}

void TaskManagerInterface::ResourceFlagsAdded(int64 flags) {
  enabled_resources_flags_ |= flags;
}

void TaskManagerInterface::SetEnabledResourceFlags(int64 flags) {
  enabled_resources_flags_ = flags;
}

void TaskManagerInterface::ScheduleRefresh(base::TimeDelta refresh_time) {
  refresh_timer_->Start(FROM_HERE,
                        refresh_time,
                        base::Bind(&TaskManagerInterface::Refresh,
                                   base::Unretained(this)));
}

}  // namespace task_management
