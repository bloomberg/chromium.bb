// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_LOG_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_METRICS_LOG_CHROMEOS_H_

#include "chrome/browser/metrics/perf_provider_chromeos.h"

namespace device {
class BluetoothAdapter;
}

namespace metrics {
class ChromeUserMetricsExtension;
}

class PrefService;

// Performs ChromeOS specific metrics logging.
class MetricsLogChromeOS {
 public:
  explicit MetricsLogChromeOS(metrics::ChromeUserMetricsExtension* uma_proto);
  virtual ~MetricsLogChromeOS();

  // Logs ChromeOS specific metrics which don't need to be updated immediately.
  void LogChromeOSMetrics();

  // Within the stability group, write ChromeOS specific attributes that need to
  // be updated asap and can't be delayed until the user decides to restart
  // chromium.  Delaying these stats would bias metrics away from happy long
  // lived chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(PrefService* pref);

 private:
  // Update the number of users logged into a multi-profile session.
  // If the number of users change while the log is open, the call invalidates
  // the user count value.
  void UpdateMultiProfileUserCount();

  // Sets the Bluetooth Adapter instance used for the WriteBluetoothProto()
  // call.
  void SetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Writes info about paired Bluetooth devices on this system.
  virtual void WriteBluetoothProto();

  metrics::PerfProvider perf_provider_;

  // Bluetooth Adapter instance for collecting information about paired devices.
  scoped_refptr<device::BluetoothAdapter> adapter_;
  metrics::ChromeUserMetricsExtension* uma_proto_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogChromeOS);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_LOG_CHROMEOS_H_
