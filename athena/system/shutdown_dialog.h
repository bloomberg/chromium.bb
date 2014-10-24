// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_SHUTDOWN_DIALOG_H_
#define ATHENA_SYSTEM_SHUTDOWN_DIALOG_H_

#include "athena/input/public/input_manager.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace athena {

// Shuts down in response to the power button being pressed.
class ShutdownDialog : public PowerButtonObserver {
 public:
  explicit ShutdownDialog();
  ~ShutdownDialog() override;

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

  // PowerButtonObserver:
  void OnPowerButtonStateChanged(PowerButtonObserver::State state) override;

  // |shutdown_warning_message_|'s parent container.
  aura::Window* warning_message_container_;

  // Shows a warning that the device is about to be shutdown.
  scoped_ptr<views::Widget> shutdown_warning_message_;

  State state_;

  base::OneShotTimer<ShutdownDialog> timer_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownDialog);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_SHUTDOWN_DIALOG_H_
