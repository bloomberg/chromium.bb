// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_

#include <map>

#include "base/sequenced_task_runner.h"
#include "chrome/browser/task_management/providers/task_provider_observer.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace task_management {

class TaskGroup;

// Defines a concrete implementation of the TaskManagerInterface.
class TaskManagerImpl :
    public TaskManagerInterface,
    public TaskProviderObserver,
    content::GpuDataManagerObserver {
 public:
  TaskManagerImpl();
  ~TaskManagerImpl() override;

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

  // task_management::TaskProviderObserver:
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;

  // content::GpuDataManagerObserver:
  void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& gpu_memory_stats) override;

 private:
  // task_management::TaskManagerInterface:
  void Refresh() override;

  TaskGroup* GetTaskGroupByTaskId(TaskId task_id) const;
  Task* GetTaskByTaskId(TaskId task_id) const;

  // Map TaskGroups by the IDs of the processes they represent.
  // Keys and values are unique (no duplicates).
  std::map<base::ProcessId, TaskGroup*> task_groups_by_proc_id_;

  // Map each task by its ID to the TaskGroup on which it resides.
  // Keys are unique but values will have duplicates (i.e. multiple tasks
  // running on the same process represented by a single TaskGroup).
  std::map<TaskId, TaskGroup*> task_groups_by_task_id_;

  // The current GPU memory usage stats that was last received from the
  // GpuDataManager.
  content::GPUVideoMemoryUsageStats gpu_memory_stats_;

  // The specific blocking pool SequencedTaskRunner that will be used to make
  // sure TaskGroupSampler posts their refreshes serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerImpl);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_MANAGER_IMPL_H_
