// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"

namespace {
// How long to wait between device counts metrics are recorded. Set to 1 hour.
const int kDeviceCountMetricThresholdMins = 60;
}  // namespace

namespace media_router {

DeviceCountMetrics::DeviceCountMetrics() : clock_(new base::DefaultClock()) {}
DeviceCountMetrics::~DeviceCountMetrics() = default;

void DeviceCountMetrics::RecordDeviceCountsIfNeeded(
    size_t available_device_count,
    size_t known_device_count) {
  base::Time now = clock_->Now();
  if (now - device_count_metrics_record_time_ <
      base::TimeDelta::FromMinutes(kDeviceCountMetricThresholdMins)) {
    return;
  }
  RecordDeviceCounts(available_device_count, known_device_count);
  device_count_metrics_record_time_ = now;
}

void DeviceCountMetrics::SetClockForTest(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

// static
const char DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount[] =
    "MediaRouter.Dial.AvailableDevicesCount";
const char DialDeviceCountMetrics::kHistogramDialKnownDeviceCount[] =
    "MediaRouter.Dial.KnownDevicesCount";

void DialDeviceCountMetrics::RecordDeviceCounts(size_t available_device_count,
                                                size_t known_device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialAvailableDeviceCount,
                           available_device_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialKnownDeviceCount, known_device_count);
}

// static
const char CastDeviceCountMetrics::kHistogramCastKnownDeviceCount[] =
    "MediaRouter.Cast.Discovery.KnownDevicesCount";
const char CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount[] =
    "MediaRouter.Cast.Discovery.ConnectedDevicesCount";
const char CastDeviceCountMetrics::kHistogramCastCachedSinksAvailableCount[] =
    "MediaRouter.Cast.Discovery.CachedSinksAvailableCount";
const char CastDeviceCountMetrics::kHistogramCastCachedSinkResolved[] =
    "MediaRouter.Cast.Discovery.CachedSinkResolved";

void CastDeviceCountMetrics::RecordDeviceCounts(size_t available_device_count,
                                                size_t known_device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramCastConnectedDeviceCount,
                           available_device_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramCastKnownDeviceCount, known_device_count);
}

void CastDeviceCountMetrics::RecordCachedSinksAvailableCount(
    size_t cached_sink_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramCastCachedSinksAvailableCount,
                           cached_sink_count);
}

void CastDeviceCountMetrics::RecordResolvedFromSource(SinkSource sink_source) {
  DCHECK_LT(sink_source, kTotalCount);
  UMA_HISTOGRAM_ENUMERATION(kHistogramCastCachedSinkResolved, sink_source,
                            kTotalCount);
}

}  // namespace media_router
