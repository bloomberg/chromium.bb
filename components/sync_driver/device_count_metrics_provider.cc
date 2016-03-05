// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_count_metrics_provider.h"

#include <algorithm>

#include "base/metrics/sparse_histogram.h"
#include "components/sync_driver/device_info_tracker.h"

namespace sync_driver {

DeviceCountMetricsProvider::DeviceCountMetricsProvider(
    const ProvideTrackersCallback& provide_trackers)
    : provide_trackers_(provide_trackers) {}

DeviceCountMetricsProvider::~DeviceCountMetricsProvider() {}

int DeviceCountMetricsProvider::MaxActiveDeviceCount() const {
  std::vector<const sync_driver::DeviceInfoTracker*> trackers;
  provide_trackers_.Run(&trackers);
  int max = 0;
  for (auto* tracker : trackers) {
    max = std::max(max, tracker->CountActiveDevices());
  }
  return max;
}

void DeviceCountMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sync.DeviceCount",
                              std::min(MaxActiveDeviceCount(), 100));
}

}  // namespace sync_driver
