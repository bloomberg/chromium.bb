// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_
#define DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "device/geolocation/geolocation_export.h"

namespace device {

// Allows sharing and mocking of the update polling policy function.
class DEVICE_GEOLOCATION_EXPORT WifiPollingPolicy {
 public:
  virtual ~WifiPollingPolicy() = default;

  // Methods for managing the single instance of WifiPollingPolicy. The WiFi
  // policy is global so it can outlive the WifiDataProvider instance, which is
  // shut down and destroyed when no WiFi scanning is active.
  static void Initialize(std::unique_ptr<WifiPollingPolicy>);
  static void Shutdown();
  static WifiPollingPolicy* Get();
  static bool IsInitialized();

  // Calculates the new polling interval for wiFi scans, given the previous
  // interval and whether the last scan produced new results.
  virtual void UpdatePollingInterval(bool scan_results_differ) = 0;
  virtual int PollingInterval() = 0;
  virtual int NoWifiInterval() = 0;

 protected:
  WifiPollingPolicy() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiPollingPolicy);
};

// Generic polling policy, constants are compile-time parameterized to allow
// tuning on a per-platform basis.
template <int DEFAULT_INTERVAL,
          int NO_CHANGE_INTERVAL,
          int TWO_NO_CHANGE_INTERVAL,
          int NO_WIFI_INTERVAL>
class GenericWifiPollingPolicy : public WifiPollingPolicy {
 public:
  GenericWifiPollingPolicy() : polling_interval_(DEFAULT_INTERVAL) {}

  // WifiPollingPolicy
  void UpdatePollingInterval(bool scan_results_differ) override {
    if (scan_results_differ) {
      polling_interval_ = DEFAULT_INTERVAL;
    } else if (polling_interval_ == DEFAULT_INTERVAL) {
      polling_interval_ = NO_CHANGE_INTERVAL;
    } else {
      DCHECK(polling_interval_ == NO_CHANGE_INTERVAL ||
             polling_interval_ == TWO_NO_CHANGE_INTERVAL);
      polling_interval_ = TWO_NO_CHANGE_INTERVAL;
    }
  }
  int PollingInterval() override { return ComputeInterval(polling_interval_); }
  int NoWifiInterval() override { return ComputeInterval(NO_WIFI_INTERVAL); }

 private:
  int ComputeInterval(int polling_interval) {
    base::Time now = base::Time::Now();

    int64_t remaining_millis = 0;
    if (!next_scan_.is_null()) {
      // Compute the remaining duration of the current interval. If the interval
      // is not yet complete, we will schedule a scan to occur once it is.
      base::TimeDelta remaining = next_scan_ - now;
      remaining_millis = remaining.InMilliseconds();
    }

    // If the current interval is complete (or if this is our first scan), scan
    // now and schedule the next scan to occur at |polling_interval|
    // milliseconds into the future.
    if (remaining_millis <= 0) {
      next_scan_ = now + base::TimeDelta::FromMilliseconds(polling_interval);
      remaining_millis = 0;
    }

    return remaining_millis;
  }

  int polling_interval_;

  // The scheduled time of the next scan, or a null value if no scan has
  // occurred yet.
  base::Time next_scan_;
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_
