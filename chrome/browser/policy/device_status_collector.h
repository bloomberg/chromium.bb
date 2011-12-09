// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
#pragma once

#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/idle.h"

using base::Time;

namespace enterprise_management {
class DeviceStatusReportRequest;
}

class PrefService;

namespace policy {

// Collects and summarizes the status of an enterprised-managed ChromeOS device.
class DeviceStatusCollector {
 public:
  explicit DeviceStatusCollector(PrefService* local_state);
  virtual ~DeviceStatusCollector();

  void GetStatus(enterprise_management::DeviceStatusReportRequest* request);

  static void RegisterPrefs(PrefService* local_state);

  // How often, in seconds, to poll to see if the user is idle.
  static const unsigned int kPollIntervalSeconds = 30;

 protected:
  // Check whether the user has been idle for a certain period of time.
  virtual void CheckIdleState();

  // Used instead of Time::Now(), to make testing possible.
  virtual Time GetCurrentTime();

  // Callback which receives the results of the idle state check.
  void IdleStateCallback(IdleState state);

  // The maximum number of active periods timestamps to be stored.
  unsigned int max_stored_active_periods_;

 private:
  void AddActivePeriod(base::Time start, base::Time end);

  // How often to poll to see if the user is idle.
  int poll_interval_seconds_;

  PrefService* local_state_;

  // The last time an idle state check was performed.
  Time last_idle_check_;

  // The idle state the last time it was checked.
  IdleState last_idle_state_;

  base::RepeatingTimer<DeviceStatusCollector> timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
