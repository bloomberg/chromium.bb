// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process/process_metrics.h"

#include "chrome/browser/performance_monitor/process_metrics_history.h"
#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif
#include "content/public/common/process_type.h"

namespace performance_monitor {

// If a process is consistently above this CPU utilization percentage over time,
// we consider it as high and may take action.
const float kHighCPUUtilizationThreshold = 90.0f;

ProcessMetricsHistory::ProcessMetricsHistory()
    : process_handle_(0),
      process_type_(content::PROCESS_TYPE_UNKNOWN),
      last_update_sequence_(0) {
  ResetCounters();
}

ProcessMetricsHistory::~ProcessMetricsHistory() {}

void ProcessMetricsHistory::ResetCounters() {
  min_cpu_usage_ = std::numeric_limits<double>::max();
  accumulated_cpu_usage_ = 0.0;
  accumulated_private_bytes_ = 0;
  accumulated_shared_bytes_ = 0;
  sample_count_ = 0;
}

void ProcessMetricsHistory::Initialize(base::ProcessHandle process_handle,
                                       int process_type,
                                       int initial_update_sequence) {
  DCHECK(process_handle_ == 0);
  process_handle_ = process_handle;
  process_type_ = process_type;
  last_update_sequence_ = initial_update_sequence;

#if defined(OS_MACOSX)
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(
      process_handle_, content::BrowserChildProcessHost::GetPortProvider()));
#else
  process_metrics_.reset(
      base::ProcessMetrics::CreateProcessMetrics(process_handle_));
#endif
}

void ProcessMetricsHistory::SampleMetrics() {
  double cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage();
  min_cpu_usage_ = std::min(min_cpu_usage_, cpu_usage);
  accumulated_cpu_usage_ += cpu_usage;

  size_t private_bytes = 0;
  size_t shared_bytes = 0;
  if (!process_metrics_->GetMemoryBytes(&private_bytes, &shared_bytes))
    LOG(WARNING) << "GetMemoryBytes returned NULL (platform-specific error)";

  accumulated_private_bytes_ += private_bytes;
  accumulated_shared_bytes_ += shared_bytes;

  sample_count_++;
}

void ProcessMetricsHistory::EndOfCycle() {
  RunPerformanceTriggers();
  ResetCounters();
}

void ProcessMetricsHistory::RunPerformanceTriggers() {
  if (sample_count_ == 0)
    return;

  // We scale up to the equivalent of 64 CPU cores fully loaded. More than this
  // doesn't really matter, as we're already in a terrible place.
  const int kHistogramMin = 0;
  const int kHistogramMax = 6400;
  const int kHistogramBucketCount = 50;

  const double average_cpu_usage = accumulated_cpu_usage_ / sample_count_;

  // The histogram macros don't support variables as histogram names,
  // hence the macro duplication for each process type.
  switch (process_type_) {
    case content::PROCESS_TYPE_BROWSER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.BrowserProcess", average_cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      // If CPU usage has consistently been above our threshold,
      // we *may* have an issue.
      if (min_cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.BrowserProcess",
                              true);
      }
      break;
    case content::PROCESS_TYPE_RENDERER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererProcess", average_cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (min_cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.RendererProcess",
                              true);
      }
      break;
    case content::PROCESS_TYPE_PLUGIN:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.PluginProcess", average_cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (min_cpu_usage_ > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PluginProcess", true);
      break;
    case content::PROCESS_TYPE_GPU:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.GPUProcess", average_cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (min_cpu_usage_ > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.GPUProcess", true);
      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.PPAPIProcess", average_cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (min_cpu_usage_ > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PPAPIProcess", true);
      break;
    default:
      break;
  }
}

}  // namespace performance_monitor
