// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_

#include "base/macros.h"

namespace chromeos {

namespace tether {

// Schedules scans for Tether hosts.
class HostScanScheduler {
 public:
  HostScanScheduler() {}
  virtual ~HostScanScheduler() {}

  // Attempts to perform a Tether host scan. If the device is already connected
  // to the internet, a scan will not be performed. If a scan is already active,
  // this function is a no-op.
  virtual void AttemptScanIfOffline() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScanScheduler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_
