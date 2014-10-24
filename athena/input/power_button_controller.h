// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_INPUT_POWER_BUTTON_CONTROLLER_H_
#define ATHENA_INPUT_POWER_BUTTON_CONTROLLER_H_

#include "athena/input/public/accelerator_manager.h"
#include "athena/input/public/input_manager.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"

namespace athena {

class PowerButtonController : public chromeos::PowerManagerClient::Observer,
                              public AcceleratorHandler {
 public:
  PowerButtonController();
  ~PowerButtonController() override;

  void AddPowerButtonObserver(PowerButtonObserver* observer);
  void RemovePowerButtonObserver(PowerButtonObserver* observer);

  void InstallAccelerators();

  // A timer callabck to notify the long press event.
  int SetPowerButtonTimeoutMsForTest(int timeout);

 private:
  // chromeos::PowerManagerClient::Observer:
  virtual void BrightnessChanged(int level, bool user_initiated) override;
  virtual void PowerButtonEventReceived(
      bool down,
      const base::TimeTicks& timestamp) override;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const override;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) override;

  void NotifyLongPress();

  int power_button_timeout_ms_;
  ObserverList<PowerButtonObserver> observers_;
  // The last time at which the screen brightness was 0%.
  base::TimeTicks zero_brightness_end_time_;
  bool brightness_is_zero_;
  base::OneShotTimer<PowerButtonController> timer_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace athena

#endif  // ATHENA_INPUT_POWER_BUTTON_CONTROLLER_H_
