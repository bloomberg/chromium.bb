// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
#define CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_

#include <assert.h>

#include "base/string16.h"
#include "base/basictypes.h"

// Converts a MAC address stored as an array of uint8 to a string.
string16 MacAddressAsString16(const uint8 mac_as_int[6]);

// Allows sharing and mocking of the update polling policy function.
class PollingPolicyInterface {
 public:
  virtual ~PollingPolicyInterface() {}
  // Calculates the new polling interval for wiFi scans, given the previous
  // interval and whether the last scan produced new results.
  virtual void UpdatePollingInterval(bool scan_results_differ) = 0;
  virtual int PollingInterval() = 0;
};

// Generic polling policy, constants are compile-time parameterized to allow
// tuning on a per-platform basis.
template<int DEFAULT_INTERVAL,
         int NO_CHANGE_INTERVAL,
         int TWO_NO_CHANGE_INTERVAL>
class GenericPollingPolicy : public PollingPolicyInterface {
 public:
  GenericPollingPolicy() : polling_interval_(DEFAULT_INTERVAL) {}
  // PollingPolicyInterface
  virtual void UpdatePollingInterval(bool scan_results_differ) {
    if (scan_results_differ) {
      polling_interval_ = DEFAULT_INTERVAL;
    } else if (polling_interval_ == DEFAULT_INTERVAL) {
      polling_interval_ = NO_CHANGE_INTERVAL;
    } else {
      assert(polling_interval_ == NO_CHANGE_INTERVAL ||
             polling_interval_ == TWO_NO_CHANGE_INTERVAL);
      polling_interval_ = TWO_NO_CHANGE_INTERVAL;
    }
  }
  virtual int PollingInterval() { return polling_interval_; }

 private:
  int polling_interval_;
};

#endif  // CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
