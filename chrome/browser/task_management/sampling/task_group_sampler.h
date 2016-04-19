// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_SAMPLER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_SAMPLER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"

namespace task_management {

// Wraps the memory usage stats values together so that it can be sent between
// the UI and the worker threads.
struct MemoryUsageStats {
  int64_t private_bytes = -1;
  int64_t shared_bytes = -1;
  int64_t physical_bytes = -1;
#if defined(OS_CHROMEOS)
  int64_t swapped_bytes = -1;
#endif

  MemoryUsageStats() {}
};

// Defines the expensive process' stats sampler that will calculate these
// resources on the worker thread. Objects of this class are created by the
// TaskGroups on the UI thread, however it will be used mainly on a blocking
// pool thread.
class TaskGroupSampler : public base::RefCountedThreadSafe<TaskGroupSampler> {
 public:
  // The below are the types of the callbacks to the UI thread upon their
  // corresponding refresh are done on the worker thread.
  using OnCpuRefreshCallback = base::Callback<void(double)>;
  using OnMemoryRefreshCallback = base::Callback<void(MemoryUsageStats)>;
  using OnIdleWakeupsCallback = base::Callback<void(int)>;
#if defined(OS_LINUX)
  using OnOpenFdCountCallback = base::Callback<void(int)>;
#endif  // defined(OS_LINUX)
  using OnProcessPriorityCallback = base::Callback<void(bool)>;

  TaskGroupSampler(
      base::Process process,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner,
      const OnCpuRefreshCallback& on_cpu_refresh,
      const OnMemoryRefreshCallback& on_memory_refresh,
      const OnIdleWakeupsCallback& on_idle_wakeups,
#if defined(OS_LINUX)
      const OnOpenFdCountCallback& on_open_fd_count,
#endif  // defined(OS_LINUX)
      const OnProcessPriorityCallback& on_process_priority);

  // Refreshes the expensive process' stats (CPU usage, memory usage, and idle
  // wakeups per second) on the worker thread.
  void Refresh(int64_t refresh_flags);

 private:
  friend class base::RefCountedThreadSafe<TaskGroupSampler>;
  ~TaskGroupSampler();

  // The refresh calls that will be done on the worker thread.
  double RefreshCpuUsage();
  MemoryUsageStats RefreshMemoryUsage();
  int RefreshIdleWakeupsPerSecond();
#if defined(OS_LINUX)
  int RefreshOpenFdCount();
#endif  // defined(OS_LINUX)
  bool RefreshProcessPriority();

  // The process that holds the handle that we own so that we can use it for
  // creating the ProcessMetrics.
  base::Process process_;

  std::unique_ptr<base::ProcessMetrics> process_metrics_;

  // The specific blocking pool SequencedTaskRunner that will be used to post
  // the refresh tasks onto serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  // The UI-thread callbacks in TaskGroup to be called when their corresponding
  // refreshes on the worker thread are done.
  const OnCpuRefreshCallback on_cpu_refresh_callback_;
  const OnMemoryRefreshCallback on_memory_refresh_callback_;
  const OnIdleWakeupsCallback on_idle_wakeups_callback_;
#if defined(OS_LINUX)
  const OnOpenFdCountCallback on_open_fd_count_callback_;
#endif  // defined(OS_LINUX)
  const OnProcessPriorityCallback on_process_priority_callback_;

  // To assert we're running on the correct thread.
  base::SequenceChecker worker_pool_sequenced_checker_;

  DISALLOW_COPY_AND_ASSIGN(TaskGroupSampler);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_SAMPLING_TASK_GROUP_SAMPLER_H_
