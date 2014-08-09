// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
#define ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_

#include "base/time/time.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/animation/tween.h"

namespace athena {

// Shuts down in response to the power button being pressed.
class PowerButtonController : public chromeos::PowerManagerClient::Observer,
                              public ui::ImplicitAnimationObserver {
 public:
  PowerButtonController();
  virtual ~PowerButtonController();

 private:
  enum State {
    // The screen is animating prior to shutdown. Shutdown can be canceled.
    STATE_PRE_SHUTDOWN_ANIMATION,

    // A D-Bus shutdown request has been sent. Shutdown cannot be canceled.
    STATE_SHUTDOWN_REQUESTED,

    STATE_OTHER
  };

  // Animates the screen's grayscale and brightness to |target|.
  void StartGrayscaleAndBrightnessAnimation(float target,
                                            int duration_ms,
                                            gfx::Tween::Type tween_type);

  // chromeos::PowerManagerClient::Observer:
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;
  virtual void PowerButtonEventReceived(
      bool down,
      const base::TimeTicks& timestamp) OVERRIDE;

  // ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // Whether the screen brightness was reduced to 0%.
  bool brightness_is_zero_;

  // The last time at which the screen brightness was 0%.
  base::TimeTicks zero_brightness_end_time_;

  State state_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
