// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
#define ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace athena {

// Shuts down in response to the power button being pressed.
class PowerButtonController : public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerButtonController(aura::Window* dialog_container);
  virtual ~PowerButtonController();

 private:
  enum State {
    // Releasing the power button sends a suspend request.
    STATE_SUSPEND_ON_RELEASE,

    // A warning that the device is about to be shutdown is visible. Releasing
    // the power button does not send a suspend or a shutdown request.
    STATE_SHUTDOWN_WARNING_VISIBLE,

    // A D-Bus shutdown request has been sent. Shutdown cannot be canceled.
    STATE_SHUTDOWN_REQUESTED,

    STATE_OTHER
  };

  // Shows the shutdown warning dialog.
  void ShowShutdownWarningDialog();

  // Requests shutdown.
  void Shutdown();

  // chromeos::PowerManagerClient::Observer:
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;
  virtual void PowerButtonEventReceived(
      bool down,
      const base::TimeTicks& timestamp) OVERRIDE;

  // |shutdown_warning_message_|'s parent container.
  aura::Window* warning_message_container_;

  // Shows a warning that the device is about to be shutdown.
  scoped_ptr<views::Widget> shutdown_warning_message_;

  // Whether the screen brightness was reduced to 0%.
  bool brightness_is_zero_;

  // The last time at which the screen brightness was 0%.
  base::TimeTicks zero_brightness_end_time_;

  State state_;

  base::OneShotTimer<PowerButtonController> timer_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_POWER_BUTTON_CONTROLLER_H_
