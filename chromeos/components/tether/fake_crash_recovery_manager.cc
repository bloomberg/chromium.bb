// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_crash_recovery_manager.h"

namespace chromeos {

namespace tether {

FakeCrashRecoveryManager::FakeCrashRecoveryManager() = default;

FakeCrashRecoveryManager::~FakeCrashRecoveryManager() = default;

void FakeCrashRecoveryManager::RestorePreCrashStateIfNecessary(
    const base::Closure& on_restoration_finished) {
  on_restoration_finished_callback_ = on_restoration_finished;
}

}  // namespace tether

}  // namespace chromeos
