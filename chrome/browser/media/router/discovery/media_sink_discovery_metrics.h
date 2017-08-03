// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_

#include <memory>

#include "base/time/clock.h"
#include "base/time/time.h"

namespace media_router {

class DeviceCountMetrics {
 public:
  DeviceCountMetrics();
  ~DeviceCountMetrics();

  // Records device counts if last record was more than one hour ago.
  void RecordDeviceCountsIfNeeded(size_t available_device_count,
                                  size_t known_device_count);

  // Allows tests to swap in a fake clock.
  void SetClockForTest(std::unique_ptr<base::Clock> clock);

 protected:
  // Record device counts.
  virtual void RecordDeviceCounts(size_t available_device_count,
                                  size_t known_device_count) = 0;

 private:
  base::Time device_count_metrics_record_time_;

  std::unique_ptr<base::Clock> clock_;
};

// Metrics for DIAL device counts.
class DialDeviceCountMetrics : public DeviceCountMetrics {
 public:
  static const char kHistogramDialAvailableDeviceCount[];
  static const char kHistogramDialKnownDeviceCount[];

  void RecordDeviceCounts(size_t available_device_count,
                          size_t known_device_count) override;
};

// Metrics for Cast device counts.
class CastDeviceCountMetrics : public DeviceCountMetrics {
 public:
  static const char kHistogramCastKnownDeviceCount[];
  static const char kHistogramCastConnectedDeviceCount[];

  void RecordDeviceCounts(size_t available_device_count,
                          size_t known_device_count) override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_
