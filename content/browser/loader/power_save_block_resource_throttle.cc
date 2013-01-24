// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/power_save_block_resource_throttle.h"

#include "content/public/browser/power_save_blocker.h"

namespace content {

PowerSaveBlockResourceThrottle::PowerSaveBlockResourceThrottle(
    const std::string& reason) {
  power_save_blocker_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension, reason);
}

PowerSaveBlockResourceThrottle::~PowerSaveBlockResourceThrottle() {
}

void PowerSaveBlockResourceThrottle::WillProcessResponse(bool* defer) {
  // Stop blocking power save after request finishes.
  power_save_blocker_.reset();
}

}  // namespace content
