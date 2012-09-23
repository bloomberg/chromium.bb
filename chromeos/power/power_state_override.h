// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
#define CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// This class overrides the current power state on the machine, disabling
// a set of power management features.
class CHROMEOS_EXPORT PowerStateOverride {
 public:
  enum Mode {
    // Blocks the screen from being dimmed or blanked due to user inactivity.
    // Also implies BLOCK_SYSTEM_SUSPEND.
    BLOCK_DISPLAY_SLEEP,

    // Blocks the system from being suspended due to user inactivity or (in the
    // case of a laptop) the lid being closed.
    BLOCK_SYSTEM_SUSPEND,
  };

  explicit PowerStateOverride(Mode mode);
  ~PowerStateOverride();

 private:
  // Callback from RequestPowerStateOverride which receives our request_id.
  void SetRequestId(uint32 request_id);

  // Actually make a call to power manager; we need this to be able to post a
  // delayed task since we cannot call back into power manager from Heartbeat
  // since the last request has just been completed at that point.
  void CallRequestPowerStateOverrides();

  // Bitmap containing requested override types from
  // PowerManagerClient::PowerStateOverrideType.
  uint32 override_types_;

  // Outstanding override request ID, or 0 if there is no outstanding request.
  uint32 request_id_;

  // Periodically invokes CallRequestPowerStateOverrides() to refresh the
  // override.
  base::RepeatingTimer<PowerStateOverride> heartbeat_;

  base::WeakPtrFactory<PowerStateOverride> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerStateOverride);
};

}  // namespace chromeos

#endif  // CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
