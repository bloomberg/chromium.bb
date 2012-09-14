// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
#define CHROME_BROWSER_CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_

#include "base/memory/weak_ptr.h"
#include "base/timer.h"

namespace chromeos {

// This class overrides the current power state on the machine, disabling
// all power management features - this includes screen dimming, screen
// blanking, suspend and suspend on lid down.
class PowerStateOverride {
 public:
  PowerStateOverride();
  ~PowerStateOverride();

 private:
  // Callback from RequestPowerStateOverride which receives our request_id.
  void SetRequestId(uint32 request_id);

  // Start our heartbeat timer.
  void StartHeartbeat();

  // Actually make a call to power manager; we need this to be able to post a
  // delayed task since we cannot call back into power manager from Heartbeat
  // since the last request has just been completed at that point.
  void CallRequestPowerStateOverrides();

  uint32 request_id_;

  base::RepeatingTimer<PowerStateOverride> heartbeat_;

  base::WeakPtrFactory<PowerStateOverride> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerStateOverride);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
