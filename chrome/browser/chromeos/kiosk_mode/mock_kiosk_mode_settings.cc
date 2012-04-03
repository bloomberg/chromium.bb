// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.h"

namespace chromeos {

bool MockKioskModeSettings::IsKioskModeEnabled() {
  return true;
}

void MockKioskModeSettings::Initialize(
    const base::Closure& notify_initialized) {
  is_initialized_ = true;
  notify_initialized.Run();
}

bool MockKioskModeSettings::is_initialized() const {
  return is_initialized_;
}

base::TimeDelta MockKioskModeSettings::GetIdleLogoutTimeout() const {
  if (!is_initialized_)
    return base::TimeDelta::FromSeconds(-1);

  return base::TimeDelta::FromMilliseconds(kMockIdleLogoutTimeoutMs);
}

base::TimeDelta MockKioskModeSettings::GetIdleLogoutWarningDuration() const {
  if (!is_initialized_)
    return base::TimeDelta::FromSeconds(-1);

  return base::TimeDelta::FromMilliseconds(kMockIdleLogoutWarningDurationMs);
}

MockKioskModeSettings::MockKioskModeSettings() : is_initialized_(false) {
}

MockKioskModeSettings::~MockKioskModeSettings() {
}

}  // namespace chromeos
