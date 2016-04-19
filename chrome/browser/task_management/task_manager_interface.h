// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
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

  // Kills the task with |task_id|.
  virtual void KillTask(TaskId task_id) = 0;

  // returns the CPU usage in percent for the process on which the task with
  // |task_id| is running during the current refresh cycle.
  virtual double GetCpuUsage(TaskId task_id) const = 0;

  // Returns the current physical/private/shared memory usage of the task with
  // |task_id| in bytes. A value of -1 means no valid value is currently
  // available.
  virtual int64_t GetPhysicalMemoryUsage(TaskId task_id) const = 0;
  virtual int64_t GetPrivateMemoryUsage(TaskId task_id) const = 0;
  virtual int64_t GetSharedMemoryUsage(TaskId task_id) const = 0;
  virtual int64_t GetSwappedMemoryUsage(TaskId task_id) const = 0;

  // Returns the GPU memory usage of the task with |task_id| in bytes. A value
  // of -1 means no valid value is currently available.
  // |has_duplicates| will be set to true if this process' GPU resource count is
  // inflated because it is counting other processes' resources.
  virtual int64_t GetGpuMemoryUsage(TaskId task_id,
                                    bool* has_duplicates) const = 0;

  // Returns the number of average idle CPU wakeups per second since the last
  // refresh cycle. A value of -1 means no valid value is currently available.
  virtual int GetIdleWakeupsPerSecond(TaskId task_id) const = 0;

  // Returns the NaCl GDB debug stub port. A value of
  // |nacl::kGdbDebugStubPortUnknown| means no valid value is currently
  // available. A value of -2 means NaCl is not enabled for this build.
  virtual int GetNaClDebugStubPort(TaskId task_id) const = 0;

  // On Windows, gets the current and peak number of GDI and USER handles in
  // use. A value of -1 means no valid value is currently available.
  virtual void GetGDIHandles(TaskId task_id,
                             int64_t* current,
                             int64_t* peak) const = 0;
  virtual void GetUSERHandles(TaskId task_id,
                              int64_t* current,
                              int64_t* peak) const = 0;

  // On Linux and ChromeOS, gets the number of file descriptors currently open
  // by the process on which the task with |task_id| is running, or -1 if no
  // valid value is currently available.
  virtual int GetOpenFdCount(TaskId task_id) const = 0;

  // Returns whether the task with |task_id| is running on a backgrounded
  // process.
  virtual bool IsTaskOnBackgroundedProcess(TaskId task_id) const = 0;

  // Returns the title of the task with |task_id|.
  virtual const base::string16& GetTitle(TaskId task_id) const = 0;

  // Returns the canonicalized name of the task with |task_id| that can be used
  // to represent this task in a Rappor sample via RapporService.
  virtual const std::string& GetTaskNameForRappor(TaskId task_id) const = 0;

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

  // Gets the unique ID of the tab if the task with |task_id| represents a
  // WebContents of a tab. Returns -1 otherwise.
  virtual int GetTabId(TaskId task_id) const = 0;

  // Returns the unique ID of the BrowserChildProcessHost/RenderProcessHost on
  // which the task with |task_id| is running. It is not the PID nor the handle
  // of the process.
  // For a task that represents the browser process, the return value is 0.
  // For a task that doesn't have a host on the browser process, the return
  // value is also 0. For other tasks that represent renderers and other child
  // processes, the return value is whatever unique IDs of their hosts in the
  // browser process.
  virtual int GetChildProcessUniqueId(TaskId task_id) const = 0;

  // If the process, in which the task with |task_id| is running, is terminated
  // this gets the termination status. Currently implemented only for Renderer
  // processes. The values will only be valid if the process has actually
  // terminated.
  virtual void GetTerminationStatus(TaskId task_id,
                                    base::TerminationStatus* out_status,
                                    int* out_error_code) const = 0;

  // Returns the network usage (in bytes per second) during the current refresh
  // cycle for the task with |task_id|. A value of -1 means no valid value is
  // currently available or that task has never been notified of any network
  // usage.
  virtual int64_t GetNetworkUsage(TaskId task_id) const = 0;

  // Returns the total network usage (in bytes per second) during the current
  // refresh cycle for the process on which the task with |task_id| is running.
  // This is the sum of all the network usage of the individual tasks (that
  // can be gotten by the above GetNetworkUsage()). A value of -1 means network
  // usage calculation refresh is currently not available.
  virtual int64_t GetProcessTotalNetworkUsage(TaskId task_id) const = 0;

  // Returns the Sqlite used memory (in bytes) for the task with |task_id|.
  // A value of -1 means no valid value is currently available.
  virtual int64_t GetSqliteMemoryUsed(TaskId task_id) const = 0;

  // Returns the allocated and used V8 memory (in bytes) for the task with
  // |task_id|. A return value of false means no valid value is currently
  // available.
  virtual bool GetV8Memory(TaskId task_id,
                           int64_t* allocated,
                           int64_t* used) const = 0;

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

  // Gets the list of task IDs of the tasks that run on the same process as the
  // task with |task_id|. The returned list will at least include |task_id|.
  virtual TaskIdList GetIdsOfTasksSharingSameProcess(TaskId task_id) const = 0;

  // Gets the number of task-manager tasks running on the same process on which
  // the Task with |task_id| is running.
  virtual size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const = 0;

  // Returns true if the resource |type| usage calculation is enabled and
  // the implementation should refresh its value (this means that at least one
  // of the observers require this value). False otherwise.
  bool IsResourceRefreshEnabled(RefreshType type) const;

 protected:
  TaskManagerInterface();
  virtual ~TaskManagerInterface();

  // Notifying observers of various events.
  void NotifyObserversOnTaskAdded(TaskId id);
  void NotifyObserversOnTaskToBeRemoved(TaskId id);
  void NotifyObserversOnRefresh(const TaskIdList& task_ids);
  void NotifyObserversOnRefreshWithBackgroundCalculations(
      const TaskIdList& task_ids);
  void NotifyObserversOnTaskUnresponsive(TaskId id);

  // Refresh all the enabled resources usage of all the available tasks.
  virtual void Refresh() = 0;

  // StartUpdating will be called once an observer is added, and StopUpdating
  // will be called when the last observer is removed.
  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

  // Returns the current refresh time that this task manager is running at. It
  // will return base::TimeDelta::Max() if the task manager is not running.
  base::TimeDelta GetCurrentRefreshTime() const;

  int64_t enabled_resources_flags() const { return enabled_resources_flags_; }

  void set_timer_for_testing(std::unique_ptr<base::Timer> timer) {
    refresh_timer_ = std::move(timer);
  }

 private:
  friend class TaskManagerObserver;

  // This should be called after each time an observer changes its own desired
  // resources refresh flags.
  void RecalculateRefreshFlags();

  // Appends |flags| to the |enabled_resources_flags_|.
  void ResourceFlagsAdded(int64_t flags);

  // Sets |enabled_resources_flags_| to |flags|.
  void SetEnabledResourceFlags(int64_t flags);

  // Schedules the task manager refresh cycles using the given |refresh_time|.
  // It stops any existing refresh schedule.
  void ScheduleRefresh(base::TimeDelta refresh_time);

  // The list of observers.
  base::ObserverList<TaskManagerObserver> observers_;

  // The timer that will be used to schedule the successive refreshes.
  std::unique_ptr<base::Timer> refresh_timer_;

  // The flags containing the enabled resources types calculations.
  int64_t enabled_resources_flags_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerInterface);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_INTERFACE_H_
