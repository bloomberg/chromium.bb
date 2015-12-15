// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/sampling/task_group_sampler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"

namespace task_management {

namespace {

base::ProcessMetrics* CreateProcessMetrics(base::ProcessHandle handle) {
#if !defined(OS_MACOSX)
  return base::ProcessMetrics::CreateProcessMetrics(handle);
#else
  return base::ProcessMetrics::CreateProcessMetrics(
      handle, content::BrowserChildProcessHost::GetPortProvider());
#endif
}

inline bool IsResourceRefreshEnabled(RefreshType refresh_type,
                                     int refresh_flags) {
  return (refresh_flags & refresh_type) != 0;
}

}  // namespace

TaskGroupSampler::TaskGroupSampler(
    base::Process process,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner,
    const OnCpuRefreshCallback& on_cpu_refresh,
    const OnMemoryRefreshCallback& on_memory_refresh,
    const OnIdleWakeupsCallback& on_idle_wakeups,
#if defined(OS_LINUX)
    const OnOpenFdCountCallback& on_open_fd_count,
#endif  // defined(OS_LINUX)
    const OnProcessPriorityCallback& on_process_priority)
    : process_(process.Pass()),
      process_metrics_(CreateProcessMetrics(process_.Handle())),
      blocking_pool_runner_(blocking_pool_runner),
      on_cpu_refresh_callback_(on_cpu_refresh),
      on_memory_refresh_callback_(on_memory_refresh),
      on_idle_wakeups_callback_(on_idle_wakeups),
#if defined(OS_LINUX)
      on_open_fd_count_callback_(on_open_fd_count),
#endif  // defined(OS_LINUX)
      on_process_priority_callback_(on_process_priority) {
  DCHECK(blocking_pool_runner.get());

  // This object will be created on the UI thread, however the sequenced checker
  // will be used to assert we're running the expensive operations on one of the
  // blocking pool threads.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  worker_pool_sequenced_checker_.DetachFromSequence();
}

void TaskGroupSampler::Refresh(int64 refresh_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsResourceRefreshEnabled(REFRESH_TYPE_CPU, refresh_flags)) {
    base::PostTaskAndReplyWithResult(
        blocking_pool_runner_.get(),
        FROM_HERE,
        base::Bind(&TaskGroupSampler::RefreshCpuUsage, this),
        on_cpu_refresh_callback_);
  }

  if (IsResourceRefreshEnabled(REFRESH_TYPE_MEMORY, refresh_flags)) {
    base::PostTaskAndReplyWithResult(
        blocking_pool_runner_.get(),
        FROM_HERE,
        base::Bind(&TaskGroupSampler::RefreshMemoryUsage, this),
        on_memory_refresh_callback_);
  }

#if defined(OS_MACOSX) || defined(OS_LINUX)
  if (IsResourceRefreshEnabled(REFRESH_TYPE_IDLE_WAKEUPS, refresh_flags)) {
    base::PostTaskAndReplyWithResult(
        blocking_pool_runner_.get(),
        FROM_HERE,
        base::Bind(&TaskGroupSampler::RefreshIdleWakeupsPerSecond, this),
        on_idle_wakeups_callback_);
  }
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_LINUX)
  if (IsResourceRefreshEnabled(REFRESH_TYPE_FD_COUNT, refresh_flags)) {
    base::PostTaskAndReplyWithResult(
        blocking_pool_runner_.get(),
        FROM_HERE,
        base::Bind(&TaskGroupSampler::RefreshOpenFdCount, this),
        on_open_fd_count_callback_);
  }
#endif  // defined(OS_LINUX)

  if (IsResourceRefreshEnabled(REFRESH_TYPE_PRIORITY, refresh_flags)) {
    base::PostTaskAndReplyWithResult(
        blocking_pool_runner_.get(),
        FROM_HERE,
        base::Bind(&TaskGroupSampler::RefreshProcessPriority, this),
        on_process_priority_callback_);
  }
}

TaskGroupSampler::~TaskGroupSampler() {
}

double TaskGroupSampler::RefreshCpuUsage() {
  DCHECK(worker_pool_sequenced_checker_.CalledOnValidSequencedThread());

  return process_metrics_->GetCPUUsage();
}

MemoryUsageStats TaskGroupSampler::RefreshMemoryUsage() {
  DCHECK(worker_pool_sequenced_checker_.CalledOnValidSequencedThread());

  // TODO(afakhry): Integrate Bruce's CL for faster retrieval of physical memory
  // on Windows here.

  MemoryUsageStats memory_usage;
  // Refreshing the physical/private/shared memory at one shot.
  base::WorkingSetKBytes ws_usage;
  if (process_metrics_->GetWorkingSetKBytes(&ws_usage)) {
    memory_usage.private_bytes = static_cast<int64>(ws_usage.priv * 1024);
    memory_usage.shared_bytes = static_cast<int64>(ws_usage.shared * 1024);
#if defined(OS_LINUX)
    // On Linux private memory is also resident. Just use it.
    memory_usage.physical_bytes = memory_usage.private_bytes;
#else
    // Memory = working_set.private which is working set minus shareable. This
    // avoids the unpredictable counting that occurs when calculating memory as
    // working set minus shared (renderer code counted when one tab is open and
    // not counted when two or more are open) and it is much more efficient to
    // calculate on Windows.
    memory_usage.physical_bytes =
        static_cast<int64>(process_metrics_->GetWorkingSetSize());
    memory_usage.physical_bytes -=
        static_cast<int64>(ws_usage.shareable * 1024);
#endif
  }

  return memory_usage;
}

int TaskGroupSampler::RefreshIdleWakeupsPerSecond() {
  DCHECK(worker_pool_sequenced_checker_.CalledOnValidSequencedThread());

  return process_metrics_->GetIdleWakeupsPerSecond();
}

#if defined(OS_LINUX)
int TaskGroupSampler::RefreshOpenFdCount() {
  DCHECK(worker_pool_sequenced_checker_.CalledOnValidSequencedThread());

  return process_metrics_->GetOpenFdCount();
}
#endif  // defined(OS_LINUX)

bool TaskGroupSampler::RefreshProcessPriority() {
  DCHECK(worker_pool_sequenced_checker_.CalledOnValidSequencedThread());

  return process_.IsProcessBackgrounded();
}

}  // namespace task_management
