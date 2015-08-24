// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_manager_interface.h"

#include "chrome/browser/task_management/sampling/task_manager_impl.h"
#include "chrome/browser/task_management/sampling/task_manager_io_thread_helper.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

namespace task_management {

// static
TaskManagerInterface* TaskManagerInterface::GetTaskManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return TaskManagerImpl::GetInstance();
}

// static
void TaskManagerInterface::OnRawBytesRead(const net::URLRequest& request,
                                          int64_t bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (switches::NewTaskManagerEnabled())
    TaskManagerIoThreadHelper::OnRawBytesRead(request, bytes_read);
  else
    TaskManager::GetInstance()->model()->NotifyBytesRead(request, bytes_read);
}

void TaskManagerInterface::AddObserver(TaskManagerObserver* observer) {
  observers_.AddObserver(observer);
  observer->observed_task_manager_ = this;

  ResourceFlagsAdded(observer->desired_resources_flags());

  base::TimeDelta current_refresh_time = GetCurrentRefreshTime();
  if (current_refresh_time == base::TimeDelta::Max()) {
    // This is the first observer to be added. Start updating.
    StartUpdating();
  }

  if (observer->desired_refresh_time() > current_refresh_time)
    return;

  // Reached here, then this is EITHER (not the first observer to be added AND
  // it requires a more frequent refresh rate) OR (it's the very first observer
  // to be added).
  // Reset the refresh timer.
  ScheduleRefresh(observer->desired_refresh_time());
}

void TaskManagerInterface::RemoveObserver(TaskManagerObserver* observer) {
  observers_.RemoveObserver(observer);
  observer->observed_task_manager_ = nullptr;

  // Recalculate the minimum refresh rate and the enabled resource flags.
  int64 flags = 0;
  base::TimeDelta min_time = base::TimeDelta::Max();
  base::ObserverList<TaskManagerObserver>::Iterator itr(&observers_);
  while (TaskManagerObserver* obs = itr.GetNext()) {
    if (obs->desired_refresh_time() < min_time)
      min_time = obs->desired_refresh_time();

    flags |= obs->desired_resources_flags();
  }

  if (min_time == base::TimeDelta::Max()) {
    // This is the last observer to be removed. Stop updating.
    SetEnabledResourceFlags(0);
    refresh_timer_->Stop();
    StopUpdating();
  } else {
    SetEnabledResourceFlags(flags);
    ScheduleRefresh(min_time);
  }
}

void TaskManagerInterface::RecalculateRefreshFlags() {
  int64 flags = 0;
  base::ObserverList<TaskManagerObserver>::Iterator itr(&observers_);
  while (TaskManagerObserver* obs = itr.GetNext())
    flags |= obs->desired_resources_flags();

  SetEnabledResourceFlags(flags);
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
