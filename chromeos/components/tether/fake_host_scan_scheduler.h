// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_

#include "base/macros.h"
#include "chromeos/components/tether/host_scan_scheduler.h"

namespace chromeos {

namespace tether {

// Test double for HostScanScheduler.
class FakeHostScanScheduler : public HostScanScheduler {
 public:
  FakeHostScanScheduler();
  ~FakeHostScanScheduler() override;

  int num_attempted_scans() { return num_attempted_scans_; }

  // HostScanScheduler:
  void AttemptScanIfOffline() override;

 private:
  int num_attempted_scans_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeHostScanScheduler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_
