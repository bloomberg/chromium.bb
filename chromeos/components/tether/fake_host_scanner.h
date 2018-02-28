// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_

#include "chromeos/components/tether/host_scanner.h"

namespace chromeos {

namespace tether {

// Test double for HostScanner.
class FakeHostScanner : public HostScanner {
 public:
  FakeHostScanner();
  ~FakeHostScanner() override;

  size_t num_scans_started() { return num_scans_started_; }

  void NotifyScanFinished();

  // HostScanner:
  bool IsScanActive() override;
  void StartScan() override;
  void StopScan() override;

 private:
  size_t num_scans_started_ = 0u;
  bool is_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeHostScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_
