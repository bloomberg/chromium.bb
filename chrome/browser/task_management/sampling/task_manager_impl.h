// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_

#include <map>

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/task_management/providers/task_provider.h"
#include "chrome/browser/task_management/providers/task_provider_observer.h"
#include "chrome/browser/task_management/sampling/task_group.h"
#include "chrome/browser/task_management/sampling/task_manager_io_thread_helper.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace task_management {

// Defines a concrete implementation of the TaskManagerInterface.
class TaskManagerImpl :
    public TaskManagerInterface,
    public TaskProviderObserver,
    content::GpuDataManagerObserver {
 public:
  ~TaskManagerImpl() override;

  static TaskManagerImpl* GetInstance();

  // task_management::TaskManagerInterface:
  void ActivateTask(TaskId task_id) override;
  double GetCpuUsage(TaskId task_id) const override;
  int64 GetPhysicalMemoryUsage(TaskId task_id) const override;
  int64 GetPrivateMemoryUsage(TaskId task_id) const override;
  int64 GetSharedMemoryUsage(TaskId task_id) const override;
  int64 GetGpuMemoryUsage(TaskId task_id, bool* has_duplicates) const override;
  int GetIdleWakeupsPerSecond(TaskId task_id) const override;
  int GetNaClDebugStubPort(TaskId task_id) const override;
  void GetGDIHandles(TaskId task_id,
                     int64* current,
                     int64* peak) const override;
  void GetUSERHandles(TaskId task_id,
                      int64* current,
                      int64* peak) const override;
  const base::string16& GetTitle(TaskId task_id) const override;
  base::string16 GetProfileName(TaskId task_id) const override;
  const gfx::ImageSkia& GetIcon(TaskId task_id) const override;
  const base::ProcessHandle& GetProcessHandle(TaskId task_id) const override;
  const base::ProcessId& GetProcessId(TaskId task_id) const override;
  Task::Type GetType(TaskId task_id) const override;
  int64 GetNetworkUsage(TaskId task_id) const override;
  int64 GetSqliteMemoryUsed(TaskId task_id) const override;
  bool GetV8Memory(TaskId task_id,
                   int64* allocated,
                   int64* used) const override;
  bool GetWebCacheStats(
      TaskId task_id,
      blink::WebCache::ResourceTypeStats* stats) const override;
  const TaskIdList& GetTaskIdsList() const override;
  size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const override;

  // task_management::TaskProviderObserver:
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;

  // content::GpuDataManagerObserver:
  void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& gpu_memory_stats) override;

  // The notification method on the UI thread when multiple bytes are read
  // from URLRequests. This will be called by the |io_thread_helper_|
  static void OnMultipleBytesReadUI(std::vector<BytesReadParam>* params);

 private:
  friend struct base::DefaultLazyInstanceTraits<TaskManagerImpl>;

  TaskManagerImpl();

  // task_management::TaskManagerInterface:
  void Refresh() override;
  void StartUpdating() override;
  void StopUpdating() override;

  // Based on |param| the appropriate task will be updated by its network usage.
  // Returns true if it was able to match |param| to an existing task, returns
  // false otherwise, at which point the caller must explicitly match these
  // bytes to the browser process by calling this method again with
  // |param.origin_pid = 0| and |param.child_id = param.route_id = -1|.
  bool UpdateTasksWithBytesRead(const BytesReadParam& param);

  TaskGroup* GetTaskGroupByTaskId(TaskId task_id) const;
  Task* GetTaskByTaskId(TaskId task_id) const;

  // Map TaskGroups by the IDs of the processes they represent.
  // Keys and values are unique (no duplicates).
  std::map<base::ProcessId, TaskGroup*> task_groups_by_proc_id_;

  // Map each task by its ID to the TaskGroup on which it resides.
  // Keys are unique but values will have duplicates (i.e. multiple tasks
  // running on the same process represented by a single TaskGroup).
  std::map<TaskId, TaskGroup*> task_groups_by_task_id_;

  // A cached sorted list of the task IDs.
  mutable std::vector<TaskId> sorted_task_ids_;

  // The manager of the IO thread helper used to handle network bytes
  // notifications on IO thread. The manager itself lives on the UI thread, but
  // the IO thread helper lives entirely on the IO thread.
  scoped_ptr<IoThreadHelperManager> io_thread_helper_manager_;

  // The list of the task providers that are owned and observed by this task
  // manager implementation.
  ScopedVector<TaskProvider> task_providers_;

  // The current GPU memory usage stats that was last received from the
  // GpuDataManager.
  content::GPUVideoMemoryUsageStats gpu_memory_stats_;

  // The specific blocking pool SequencedTaskRunner that will be used to make
  // sure TaskGroupSampler posts their refreshes serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  // This will be set to true while there are observers and the task manager is
  // running.
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerImpl);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_
