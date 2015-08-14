// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/task_management/providers/task.h"
#include "chrome/browser/task_management/sampling/task_group_sampler.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "content/public/common/gpu_memory_stats.h"

namespace task_management {

// Defines a group of tasks tracked by the task manager which belong to the same
// process. This class lives on the UI thread.
class TaskGroup {
 public:
  TaskGroup(
      base::ProcessHandle proc_handle,
      base::ProcessId proc_id,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner);
  ~TaskGroup();

  // Adds and removes the given |task| to this group. |task| must be running on
  // the same process represented by this group.
  void AddTask(Task* task);
  void RemoveTask(Task* task);

  void Refresh(const content::GPUVideoMemoryUsageStats& gpu_memory_stats,
               base::TimeDelta update_interval,
               int64 refresh_flags);

  // Appends the sorted IDs of the tasks that belong to this group to
  // |out_list|.
  void AppendSortedTaskIds(TaskIdList* out_list) const;

  Task* GetTaskById(TaskId task_id) const;

  const base::ProcessHandle& process_handle() const { return process_handle_; }
  const base::ProcessId& process_id() const { return process_id_; }

  size_t num_tasks() const { return tasks_.size(); }
  bool empty() const { return tasks_.empty(); }

  double cpu_usage() const { return cpu_usage_; }
  int64 private_bytes() const { return memory_usage_.private_bytes; }
  int64 shared_bytes() const { return memory_usage_.shared_bytes; }
  int64 physical_bytes() const { return memory_usage_.physical_bytes; }
  int64 gpu_memory() const { return gpu_memory_; }
  bool gpu_memory_has_duplicates() const { return gpu_memory_has_duplicates_; }

#if defined(OS_WIN)
  int64 gdi_current_handles() const { return gdi_current_handles_; }
  int64 gdi_peak_handles() const { return gdi_peak_handles_; }
  int64 user_current_handles() const { return user_current_handles_; }
  int64 user_peak_handles() const { return user_peak_handles_; }
#endif  // defined(OS_WIN)

#if !defined(DISABLE_NACL)
  int nacl_debug_stub_port() const { return nacl_debug_stub_port_; }
#endif  // !defined(DISABLE_NACL)

  int idle_wakeups_per_second() const { return idle_wakeups_per_second_; }

 private:
  void RefreshGpuMemory(
      const content::GPUVideoMemoryUsageStats& gpu_memory_stats);

  void RefreshWindowsHandles();

  // |child_process_unique_id| see Task::GetChildProcessUniqueID().
  void RefreshNaClDebugStubPort(int child_process_unique_id);

  void OnCpuRefreshDone(double cpu_usage);

  void OnMemoryUsageRefreshDone(MemoryUsageStats memory_usage);

  void OnIdleWakeupsRefreshDone(int idle_wakeups_per_second);

  // The process' handle and ID.
  base::ProcessHandle process_handle_;
  base::ProcessId process_id_;

  scoped_refptr<TaskGroupSampler> worker_thread_sampler_;

  // Maps the Tasks by their IDs.
  // Tasks are not owned by the TaskGroup. They're owned by the TaskProviders.
  std::map<TaskId, Task*> tasks_;

  // The per process resources usages.
  double cpu_usage_;
  MemoryUsageStats memory_usage_;
  int64 gpu_memory_;
#if defined(OS_WIN)
  // Windows GDI and USER Handles.
  int64 gdi_current_handles_;
  int64 gdi_peak_handles_;
  int64 user_current_handles_;
  int64 user_peak_handles_;
#endif  // defined(OS_WIN)
#if !defined(DISABLE_NACL)
  int nacl_debug_stub_port_;
#endif  // !defined(DISABLE_NACL)
  int idle_wakeups_per_second_;
  bool gpu_memory_has_duplicates_;

  // Always keep this the last member of this class so that it's the first to be
  // destroyed.
  base::WeakPtrFactory<TaskGroup> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskGroup);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_H_
