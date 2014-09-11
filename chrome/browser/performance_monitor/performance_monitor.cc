// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor.h"

#include "base/memory/singleton.h"
#include "base/process/process_iterator.h"
#include "base/time/time.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace {

// The default interval at which PerformanceMonitor performs its timed
// collections.
const int kGatherIntervalInSeconds = 120;
}

namespace performance_monitor {

PerformanceMonitor::PerformanceMonitor() {
}

PerformanceMonitor::~PerformanceMonitor() {
}

// static
PerformanceMonitor* PerformanceMonitor::GetInstance() {
  return Singleton<PerformanceMonitor>::get();
}

void PerformanceMonitor::StartGatherCycle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromSeconds(kGatherIntervalInSeconds),
                         this,
                         &PerformanceMonitor::GatherMetricsMapOnUIThread);
}

void PerformanceMonitor::GatherMetricsMapOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static int current_update_sequence = 0;
  // Even in the "somewhat" unlikely event this wraps around,
  // it doesn't matter. We just check it for inequality.
  current_update_sequence++;

  // Find all render child processes; has to be done on the UI thread.
  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    base::ProcessHandle handle = rph_iter.GetCurrentValue()->GetHandle();
    MarkProcessAsAlive(handle, content::PROCESS_TYPE_RENDERER,
                       current_update_sequence);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::GatherMetricsMapOnIOThread,
                 base::Unretained(this),
                 current_update_sequence));
}

void PerformanceMonitor::MarkProcessAsAlive(const base::ProcessHandle& handle,
                                            int process_type,
                                            int current_update_sequence) {
  if (handle == 0) {
    // Process may not be valid yet.
    return;
  }

  MetricsMap::iterator process_metrics_iter = metrics_map_.find(handle);
  if (process_metrics_iter == metrics_map_.end()) {
    // If we're not already watching the process, let's initialize it.
    metrics_map_[handle]
        .Initialize(handle, process_type, current_update_sequence);
  } else {
    // If we are watching the process, touch it to keep it alive.
    ProcessMetricsHistory& process_metrics = process_metrics_iter->second;
    process_metrics.set_last_update_sequence(current_update_sequence);
  }
}

void PerformanceMonitor::GatherMetricsMapOnIOThread(
    int current_update_sequence) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Find all child processes (does not include renderers), which has to be
  // done on the IO thread.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    const content::ChildProcessData& child_process_data = iter.GetData();
    base::ProcessHandle handle = child_process_data.handle;
    MarkProcessAsAlive(handle, child_process_data.process_type,
                       current_update_sequence);
  }

  // Add the current (browser) process.
  MarkProcessAsAlive(base::GetCurrentProcessHandle(),
                     content::PROCESS_TYPE_BROWSER, current_update_sequence);

  double cpu_usage = 0.0;
  size_t private_memory_sum = 0;
  size_t shared_memory_sum = 0;

  // Update metrics for all watched processes; remove dead entries from the map.
  MetricsMap::iterator iter = metrics_map_.begin();
  while (iter != metrics_map_.end()) {
    ProcessMetricsHistory& process_metrics = iter->second;
    if (process_metrics.last_update_sequence() != current_update_sequence) {
      // Not touched this iteration; let's get rid of it.
      metrics_map_.erase(iter++);
    } else {
      process_metrics.SampleMetrics();

      // Gather averages of previously sampled metrics.
      cpu_usage += process_metrics.GetAverageCPUUsage();

      size_t private_memory = 0;
      size_t shared_memory = 0;
      process_metrics.GetAverageMemoryBytes(&private_memory, &shared_memory);
      private_memory_sum += private_memory;
      shared_memory_sum += shared_memory;

      process_metrics.EndOfCycle();

      ++iter;
    }
  }
}

}  // namespace performance_monitor
