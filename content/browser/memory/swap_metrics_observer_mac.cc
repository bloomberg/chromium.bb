// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_observer_mac.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "base/mac/mach_logging.h"
#include "base/metrics/histogram_macros.h"

namespace content {

// static
SwapMetricsObserver* SwapMetricsObserver::GetInstance() {
  static SwapMetricsObserverMac* instance = new SwapMetricsObserverMac();
  return instance;
}

SwapMetricsObserverMac::SwapMetricsObserverMac() : host_(mach_host_self()) {}

SwapMetricsObserverMac::~SwapMetricsObserverMac() {}

void SwapMetricsObserverMac::UpdateMetricsInternal(base::TimeDelta interval) {
  vm_statistics64_data_t statistics;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  kern_return_t result =
      host_statistics64(host_.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&statistics), &count);
  if (result != KERN_SUCCESS) {
    MACH_DLOG(WARNING, result) << "host_statistics64";
    Stop();
    return;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

  double swapins = statistics.swapins - last_swapins_;
  double swapouts = statistics.swapouts - last_swapouts_;
  double decompressions = statistics.decompressions - last_decompressions_;
  double compressions = statistics.compressions - last_compressions_;
  last_swapins_ = statistics.swapins;
  last_swapouts_ = statistics.swapouts;
  last_decompressions_ = statistics.decompressions;
  last_compressions_ = statistics.compressions;

  if (interval.is_zero())
    return;

  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.SwapInPerSecond",
                             swapins / interval.InSecondsF());
  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.SwapOutPerSecond",
                             swapouts / interval.InSecondsF());
  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.DecompressedPagesPerSecond",
                             decompressions / interval.InSecondsF());
  UMA_HISTOGRAM_COUNTS_10000("Memory.Experimental.CompressedPagesPerSecond",
                             compressions / interval.InSecondsF());
}

}  // namespace content
