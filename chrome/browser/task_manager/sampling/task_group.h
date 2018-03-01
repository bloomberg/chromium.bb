// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "chrome/browser/task_manager/sampling/task_group_sampler.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "components/nacl/common/buildflags.h"

namespace gpu {
struct VideoMemoryUsageStats;
}

namespace task_manager {

class SharedSampler;

// Defines a group of tasks tracked by the task manager which belong to the same
// process. This class lives on the UI thread.
class TaskGroup {
 public:
  TaskGroup(
      base::ProcessHandle proc_handle,
      base::ProcessId proc_id,
      const base::Closure& on_background_calculations_done,
      const scoped_refptr<SharedSampler>& shared_sampler,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner);
  ~TaskGroup();

  // Adds and removes the given |task| to this group. |task| must be running on
  // the same process represented by this group.
  void AddTask(Task* task);
  void RemoveTask(Task* task);

  void Refresh(const gpu::VideoMemoryUsageStats& gpu_memory_stats,
               base::TimeDelta update_interval,
               int64_t refresh_flags);

  Task* GetTaskById(TaskId task_id) const;

  // This is to be called after the task manager had informed its observers with
  // OnTasksRefreshedWithBackgroundCalculations() to begin another cycle for
  // this notification type.
  void ClearCurrentBackgroundCalculationsFlags();

  // True if all enabled background operations calculating resource usage of the
  // process represented by this TaskGroup have completed.
  bool AreBackgroundCalculationsDone() const;

  const base::ProcessHandle& process_handle() const { return process_handle_; }
  const base::ProcessId& process_id() const { return process_id_; }

  const std::vector<Task*>& tasks() const { return tasks_; }
  size_t num_tasks() const { return tasks().size(); }
  bool empty() const { return tasks().empty(); }

  double platform_independent_cpu_usage() const {
    return platform_independent_cpu_usage_;
  }
  base::Time start_time() const { return start_time_; }
  base::TimeDelta cpu_time() const { return cpu_time_; }
  void set_footprint_bytes(int64_t footprint) { memory_footprint_ = footprint; }
  int64_t footprint_bytes() const { return memory_footprint_; }
  int64_t private_bytes() const { return memory_usage_.private_bytes; }
  int64_t shared_bytes() const { return memory_usage_.shared_bytes; }
  int64_t physical_bytes() const { return memory_usage_.physical_bytes; }
#if defined(OS_CHROMEOS)
  int64_t swapped_bytes() const { return memory_usage_.swapped_bytes; }
#endif
  int64_t gpu_memory() const { return gpu_memory_; }
  bool gpu_memory_has_duplicates() const { return gpu_memory_has_duplicates_; }
  base::MemoryState memory_state() const { return memory_state_; }
  int64_t per_process_network_usage_rate() const {
    return per_process_network_usage_rate_;
  }
  int64_t cumulative_per_process_network_usage() const {
    return cumulative_per_process_network_usage_;
  }
  bool is_backgrounded() const { return is_backgrounded_; }

#if defined(OS_WIN)
  int64_t gdi_current_handles() const { return gdi_current_handles_; }
  int64_t gdi_peak_handles() const { return gdi_peak_handles_; }
  int64_t user_current_handles() const { return user_current_handles_; }
  int64_t user_peak_handles() const { return user_peak_handles_; }
  int64_t hard_faults_per_second() const { return hard_faults_per_second_; }
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_NACL)
  int nacl_debug_stub_port() const { return nacl_debug_stub_port_; }
#endif  // BUILDFLAG(ENABLE_NACL)

#if defined(OS_LINUX)
  int open_fd_count() const { return open_fd_count_; }
#endif  // defined(OS_LINUX)

  int idle_wakeups_per_second() const { return idle_wakeups_per_second_; }

 private:
  void RefreshGpuMemory(const gpu::VideoMemoryUsageStats& gpu_memory_stats);

  void RefreshWindowsHandles();

#if BUILDFLAG(ENABLE_NACL)
  // |child_process_unique_id| see Task::GetChildProcessUniqueID().
  void RefreshNaClDebugStubPort(int child_process_unique_id);
  void OnRefreshNaClDebugStubPortDone(int port);
#endif
#if defined(OS_LINUX)
  void OnOpenFdCountRefreshDone(int open_fd_count);
#endif  // defined(OS_LINUX)

  void OnCpuRefreshDone(double cpu_usage);
  void OnMemoryUsageRefreshDone(MemoryUsageStats memory_usage);
  void OnProcessPriorityDone(bool is_backgrounded);
  void OnIdleWakeupsRefreshDone(int idle_wakeups_per_second);

  void OnSamplerRefreshDone(
      base::Optional<SharedSampler::SamplingResult> results);

  void OnBackgroundRefreshTypeFinished(int64_t finished_refresh_type);

  // The process' handle and ID.
  base::ProcessHandle process_handle_;
  base::ProcessId process_id_;

  // This is a callback into the TaskManagerImpl to inform it that the
  // background calculations for this TaskGroup has finished.
  const base::Closure on_background_calculations_done_;

  scoped_refptr<TaskGroupSampler> worker_thread_sampler_;

  scoped_refptr<SharedSampler> shared_sampler_;

  // Lists the Tasks in this TaskGroup.
  // Tasks are not owned by the TaskGroup. They're owned by the TaskProviders.
  std::vector<Task*> tasks_;

  // Flags will be used to determine when the background calculations has
  // completed for the enabled refresh types for this TaskGroup.
  int64_t expected_on_bg_done_flags_;
  int64_t current_on_bg_done_flags_;

  // The per process resources usages.
  double platform_independent_cpu_usage_;
  base::Time start_time_;     // Only calculated On Windows now.
  base::TimeDelta cpu_time_;  // Only calculated On Windows now.
  MemoryUsageStats memory_usage_;
  int64_t memory_footprint_;
  int64_t gpu_memory_;
  base::MemoryState memory_state_;
  // The network usage in bytes per second as the sum of all network usages of
  // the individual tasks sharing the same process.
  int64_t per_process_network_usage_rate_;

  // A continuously updating sum of all bytes that have been downloaded and
  // uploaded by all tasks in this process.
  int64_t cumulative_per_process_network_usage_;

#if defined(OS_WIN)
  // Windows GDI and USER Handles.
  int64_t gdi_current_handles_;
  int64_t gdi_peak_handles_;
  int64_t user_current_handles_;
  int64_t user_peak_handles_;
  int64_t hard_faults_per_second_;
#endif  // defined(OS_WIN)
#if BUILDFLAG(ENABLE_NACL)
  int nacl_debug_stub_port_;
#endif  // BUILDFLAG(ENABLE_NACL)
#if defined(OS_LINUX)
  // The number of file descriptors currently open by the process.
  int open_fd_count_;
#endif  // defined(OS_LINUX)
  int idle_wakeups_per_second_;
  bool gpu_memory_has_duplicates_;
  bool is_backgrounded_;

  // Always keep this the last member of this class so that it's the first to be
  // destroyed.
  base::WeakPtrFactory<TaskGroup> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskGroup);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_
