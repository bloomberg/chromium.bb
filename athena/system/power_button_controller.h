// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
#define ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_

#include "chromeos/dbus/power_manager_client.h"

namespace athena {

// Shuts down in response to the power button being pressed.
class PowerButtonController : public chromeos::PowerManagerClient::Observer {
 public:
  PowerButtonController();
  virtual ~PowerButtonController();

  // chromeos::PowerManagerClient::Observer:
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;
  virtual void PowerButtonEventReceived(
      bool down,
      const base::TimeTicks& timestamp) OVERRIDE;

 private:
  // Whether the screen brightness was reduced to 0%.
  bool brightness_is_zero_;

  // The last time at which the screen brightness was 0%.
  base::TimeTicks zero_brightness_end_time_;

  // Whether shutdown was requested.
  bool shutdown_requested_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
