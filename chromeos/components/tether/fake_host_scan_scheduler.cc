// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_host_scan_scheduler.h"

namespace chromeos {

namespace tether {

FakeHostScanScheduler::FakeHostScanScheduler() = default;

FakeHostScanScheduler::~FakeHostScanScheduler() = default;

void FakeHostScanScheduler::AttemptScanIfOffline() {
  ++num_attempted_scans_;
}

}  // namespace tether

}  // namespace chromeos
