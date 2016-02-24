// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/sampling/task_group.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "components/nacl/browser/nacl_browser.h"
#include "content/public/browser/browser_thread.h"

namespace task_management {

namespace {

inline bool IsResourceRefreshEnabled(RefreshType refresh_type,
                                     int refresh_flags) {
  return (refresh_flags & refresh_type) != 0;
}

#if defined(OS_WIN)
// Gets the GDI and USER Handles on Windows at one shot.
void GetWindowsHandles(base::ProcessHandle handle,
                       int64_t* out_gdi_current,
                       int64_t* out_gdi_peak,
                       int64_t* out_user_current,
                       int64_t* out_user_peak) {
  *out_gdi_current = 0;
  *out_gdi_peak = 0;
  *out_user_current = 0;
  *out_user_peak = 0;
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, handle, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    *out_gdi_current = static_cast<int64_t>(
        GetGuiResources(process_with_query_rights, GR_GDIOBJECTS));
    *out_gdi_peak = static_cast<int64_t>(
        GetGuiResources(process_with_query_rights, GR_GDIOBJECTS_PEAK));
    *out_user_current = static_cast<int64_t>(
        GetGuiResources(process_with_query_rights, GR_USEROBJECTS));
    *out_user_peak = static_cast<int64_t>(
        GetGuiResources(process_with_query_rights, GR_USEROBJECTS_PEAK));
    CloseHandle(process_with_query_rights);
  }
}
#endif  // defined(OS_WIN)

}  // namespace

TaskGroup::TaskGroup(
    base::ProcessHandle proc_handle,
    base::ProcessId proc_id,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner)
    : process_handle_(proc_handle),
      process_id_(proc_id),
      worker_thread_sampler_(nullptr),
      tasks_(),
      cpu_usage_(0.0),
      memory_usage_(),
      gpu_memory_(-1),
      per_process_network_usage_(-1),
#if defined(OS_WIN)
      gdi_current_handles_(-1),
      gdi_peak_handles_(-1),
      user_current_handles_(-1),
      user_peak_handles_(-1),
#endif  // defined(OS_WIN)
#if !defined(DISABLE_NACL)
      nacl_debug_stub_port_(-1),
#endif  // !defined(DISABLE_NACL)
      idle_wakeups_per_second_(-1),
#if defined(OS_LINUX)
      open_fd_count_(-1),
#endif  // defined(OS_LINUX)
      gpu_memory_has_duplicates_(false),
      is_backgrounded_(false),
      weak_ptr_factory_(this) {
  scoped_refptr<TaskGroupSampler> sampler(
      new TaskGroupSampler(base::Process::Open(proc_id),
                           blocking_pool_runner,
                           base::Bind(&TaskGroup::OnCpuRefreshDone,
                                      weak_ptr_factory_.GetWeakPtr()),
                           base::Bind(&TaskGroup::OnMemoryUsageRefreshDone,
                                      weak_ptr_factory_.GetWeakPtr()),
                           base::Bind(&TaskGroup::OnIdleWakeupsRefreshDone,
                                      weak_ptr_factory_.GetWeakPtr()),
#if defined(OS_LINUX)
                           base::Bind(&TaskGroup::OnOpenFdCountRefreshDone,
                                      weak_ptr_factory_.GetWeakPtr()),
#endif  // defined(OS_LINUX)
                           base::Bind(&TaskGroup::OnProcessPriorityDone,
                                      weak_ptr_factory_.GetWeakPtr())));
  worker_thread_sampler_.swap(sampler);
}

TaskGroup::~TaskGroup() {
}

void TaskGroup::AddTask(Task* task) {
  DCHECK(task);
  DCHECK(task->process_id() == process_id_);
  DCHECK(!ContainsKey(tasks_, task->task_id()));

  tasks_[task->task_id()] = task;
}

void TaskGroup::RemoveTask(Task* task) {
  DCHECK(task);
  DCHECK(ContainsKey(tasks_, task->task_id()));

  tasks_.erase(task->task_id());
}

void TaskGroup::Refresh(
    const content::GPUVideoMemoryUsageStats& gpu_memory_stats,
    base::TimeDelta update_interval,
    int64_t refresh_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // First refresh the enabled non-expensive resources usages on the UI thread.
  // 1- Refresh all the tasks as well as the total network usage (if enabled).
  const bool network_usage_refresh_enabled =
      IsResourceRefreshEnabled(REFRESH_TYPE_NETWORK_USAGE, refresh_flags);
  per_process_network_usage_ = network_usage_refresh_enabled ? 0 : -1;
  for (auto& task_pair : tasks_) {
    Task* task = task_pair.second;
    task->Refresh(update_interval, refresh_flags);

    if (network_usage_refresh_enabled && task->ReportsNetworkUsage()) {
      per_process_network_usage_ += task->network_usage();
    }
  }

  // 2- Refresh GPU memory (if enabled).
  if (IsResourceRefreshEnabled(REFRESH_TYPE_GPU_MEMORY, refresh_flags))
    RefreshGpuMemory(gpu_memory_stats);

  // 3- Refresh Windows handles (if enabled).
#if defined(OS_WIN)
  if (IsResourceRefreshEnabled(REFRESH_TYPE_HANDLES, refresh_flags))
    RefreshWindowsHandles();
#endif  // defined(OS_WIN)

  // 4- Refresh the NACL debug stub port (if enabled).
#if !defined(DISABLE_NACL)
  if (IsResourceRefreshEnabled(REFRESH_TYPE_NACL, refresh_flags) &&
      !tasks_.empty()) {
    RefreshNaClDebugStubPort(tasks_.begin()->second->GetChildProcessUniqueID());
  }
#endif  // !defined(DISABLE_NACL)

  // The remaining resource refreshes are time consuming and cannot be done on
  // the UI thread. Do them all on the worker thread using the TaskGroupSampler.
  // 5- CPU usage.
  // 6- Memory usage.
  // 7- Idle Wakeups per second.
  // 8- (Linux and ChromeOS only) The number of file descriptors current open.
  // 9- Process priority (foreground vs. background).
  worker_thread_sampler_->Refresh(refresh_flags);
}

void TaskGroup::AppendSortedTaskIds(TaskIdList* out_list) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(out_list);

  for (const auto& task_pair : tasks_)
    out_list->push_back(task_pair.first);
}

Task* TaskGroup::GetTaskById(TaskId task_id) const {
  DCHECK(ContainsKey(tasks_, task_id));

  return tasks_.at(task_id);
}

void TaskGroup::RefreshGpuMemory(
    const content::GPUVideoMemoryUsageStats& gpu_memory_stats) {
  auto itr = gpu_memory_stats.process_map.find(process_id_);
  if (itr == gpu_memory_stats.process_map.end()) {
    gpu_memory_ = -1;
    gpu_memory_has_duplicates_ = false;
    return;
  }

  gpu_memory_ = itr->second.video_memory;
  gpu_memory_has_duplicates_ = itr->second.has_duplicates;
}

void TaskGroup::RefreshWindowsHandles() {
#if defined(OS_WIN)
  GetWindowsHandles(process_handle_,
                    &gdi_current_handles_,
                    &gdi_peak_handles_,
                    &user_current_handles_,
                    &user_peak_handles_);
#endif  // defined(OS_WIN)
}

void TaskGroup::RefreshNaClDebugStubPort(int child_process_unique_id) {
#if !defined(DISABLE_NACL)
  nacl::NaClBrowser* nacl_browser = nacl::NaClBrowser::GetInstance();
  nacl_debug_stub_port_ =
      nacl_browser->GetProcessGdbDebugStubPort(child_process_unique_id);
#endif // !defined(DISABLE_NACL)
}

void TaskGroup::OnCpuRefreshDone(double cpu_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  cpu_usage_ = cpu_usage;
}

void TaskGroup::OnMemoryUsageRefreshDone(MemoryUsageStats memory_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  memory_usage_ = memory_usage;
}

void TaskGroup::OnIdleWakeupsRefreshDone(int idle_wakeups_per_second) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  idle_wakeups_per_second_ = idle_wakeups_per_second;
}

#if defined(OS_LINUX)
void TaskGroup::OnOpenFdCountRefreshDone(int open_fd_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  open_fd_count_ = open_fd_count;
}
#endif  // defined(OS_LINUX)

void TaskGroup::OnProcessPriorityDone(bool is_backgrounded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_backgrounded_ = is_backgrounded;
}

}  // namespace task_management
