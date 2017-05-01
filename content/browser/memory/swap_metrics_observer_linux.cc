// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_observer_linux.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"

namespace content {

namespace {

bool HasSwap() {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info))
    return false;
  return memory_info.swap_total > 0;
}

}  // namespace

// static
SwapMetricsObserver* SwapMetricsObserver::GetInstance() {
  static SwapMetricsObserverLinux* instance =
      HasSwap() ? new SwapMetricsObserverLinux() : nullptr;
  return instance;
}

SwapMetricsObserverLinux::SwapMetricsObserverLinux() {}

SwapMetricsObserverLinux::~SwapMetricsObserverLinux() {}

void SwapMetricsObserverLinux::UpdateMetricsInternal(base::TimeDelta interval) {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info)) {
    Stop();
    return;
  }

  double in_counts = memory_info.pswpin - last_pswpin_;
  double out_counts = memory_info.pswpout - last_pswpout_;
  last_pswpin_ = memory_info.pswpin;
  last_pswpout_ = memory_info.pswpout;

  if (interval.is_zero())
    return;

  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.SwapInPerSecond",
                             in_counts / interval.InSecondsF());
  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.SwapOutPerSecond",
                             out_counts / interval.InSecondsF());
}

}  // namespace content
