// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/task_management/providers/task.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "ui/gfx/image/image_skia.h"

namespace net {
class URLRequest;
}  // namespace net

namespace task_management {

// Defines the interface for any implementation of the task manager.
// Concrete implementations have no control over the refresh rate nor the
// enabled calculations of the usage of the various resources.
class TaskManagerInterface {
 public:
  // Gets the existing instance of the task manager if any, otherwise it will
  // create it first. Must be called on the UI thread.
  static TaskManagerInterface* GetTaskManager();

  // This notification will be received on the IO thread from
  // ChromeNetworkDelegate to update the task manager with network usage.
  static void OnRawBytesRead(const net::URLRequest& request,
                             int64_t bytes_read);

  void AddObserver(TaskManagerObserver* observer);
  void RemoveObserver(TaskManagerObserver* observer);

  // Activates the task with |task_id| by bringing its container to the front if
  // possible.
  virtual void ActivateTask(TaskId task_id) = 0;

  // returns the CPU usage in percent for the process on which the task with
  // |task_id| is running during the current refresh cycle.
  virtual double GetCpuUsage(TaskId task_id) const = 0;

  // Returns the current physical/private/shared memory usage of the task with
  // |task_id| in bytes. A value of -1 means no valid value is currently
  // available.
  virtual int64 GetPhysicalMemoryUsage(TaskId task_id) const = 0;
  virtual int64 GetPrivateMemoryUsage(TaskId task_id) const = 0;
  virtual int64 GetSharedMemoryUsage(TaskId task_id) const = 0;

  // Returns the GPU memory usage of the task with |task_id| in bytes. A value
  // of -1 means no valid value is currently available.
  // |has_duplicates| will be set to true if this process' GPU resource count is
  // inflated because it is counting other processes' resources.
  virtual int64 GetGpuMemoryUsage(TaskId task_id,
                                  bool* has_duplicates) const = 0;

  // Returns the number of average idle CPU wakeups per second since the last
  // refresh cycle. A value of -1 means no valid value is currently available.
  virtual int GetIdleWakeupsPerSecond(TaskId task_id) const = 0;

  // Returns the NaCl GDB debug stub port. A value of -1 means no valid value is
  // currently available.
  virtual int GetNaClDebugStubPort(TaskId task_id) const = 0;

  // On Windows, gets the current and peak number of GDI and USER handles in
  // use. A value of -1 means no valid value is currently available.
  virtual void GetGDIHandles(TaskId task_id,
                             int64* current,
                             int64* peak) const = 0;
  virtual void GetUSERHandles(TaskId task_id,
                              int64* current,
                              int64* peak) const = 0;

  // Returns the title of the task with |task_id|.
  virtual const base::string16& GetTitle(TaskId task_id) const = 0;

  // Returns the name of the profile associated with the browser context of the
  // render view host that the task with |task_id| represents (if that task
  // represents a renderer).
  virtual base::string16 GetProfileName(TaskId task_id) const = 0;

  // Returns the favicon of the task with |task_id|.
  virtual const gfx::ImageSkia& GetIcon(TaskId task_id) const = 0;

  // Returns the ID and handle of the process on which the task with |task_id|
  // is running.
  virtual const base::ProcessHandle& GetProcessHandle(TaskId task_id) const = 0;
  virtual const base::ProcessId& GetProcessId(TaskId task_id) const = 0;

  // Returns the type of the task with |task_id|.
  virtual Task::Type GetType(TaskId task_id) const = 0;

  // Returns the network usage (in bytes per second) during the current refresh
  // cycle for the task with |task_id|. A value of -1 means no valid value is
  // currently available or that task has never been notified of any network
  // usage.
  virtual int64 GetNetworkUsage(TaskId task_id) const = 0;

  // Returns the Sqlite used memory (in bytes) for the task with |task_id|.
  // A value of -1 means no valid value is currently available.
  virtual int64 GetSqliteMemoryUsed(TaskId task_id) const = 0;

  // Returns the allocated and used V8 memory (in bytes) for the task with
  // |task_id|. A return value of false means no valid value is currently
  // available.
  virtual bool GetV8Memory(TaskId task_id,
                           int64* allocated,
                           int64* used) const = 0;

  // Gets the Webkit resource cache stats for the task with |task_id|.
  // A return value of false means that task does NOT report WebCache stats.
  virtual bool GetWebCacheStats(
      TaskId task_id,
      blink::WebCache::ResourceTypeStats* stats) const = 0;

  // Gets the list of task IDs currently tracked by the task manager. The list
  // will be sorted such that the task representing the browser process is at
  // the top of the list and the rest of the IDs will be sorted by the process
  // IDs on which the tasks are running, then by the task IDs themselves.
  virtual const TaskIdList& GetTaskIdsList() const = 0;

  // Gets the number of task-manager tasks running on the same process on which
  // the Task with |task_id| is running.
  virtual size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const = 0;

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

  // StartUpdating will be called once an observer is added, and StopUpdating
  // will be called when the last observer is removed.
  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

  // Returns the current refresh time that this task manager is running at. It
  // will return base::TimeDelta::Max() if the task manager is not running.
  base::TimeDelta GetCurrentRefreshTime() const;

  int64 enabled_resources_flags() const { return enabled_resources_flags_; }

  void set_timer_for_testing(scoped_ptr<base::Timer> timer) {
    refresh_timer_ = timer.Pass();
  }

 private:
  friend class TaskManagerObserver;

  // This should be called after each time an observer changes its own desired
  // resources refresh flags.
  void RecalculateRefreshFlags();

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
