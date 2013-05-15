// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_
#define ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_

#include "ash/ash_export.h"
#include "base/timer.h"

namespace views {
class Widget;
}

namespace ash {

// In order to avoid accidental exits when the user presses the exit
// shortcut by mistake, we require the user to hold the shortcut for
// a period of time. During that time we show a popup informing the
// user of this. We exit only if the user holds the shortcut longer
// than this time limit.
// An expert user may quickly release and then press again (double press)
// for immediate exit. The press, release and press must happen within
// the double press time limit.
// If the user releases (without double press) before the required hold
// time, we will cancel the exit, but show the ui until the hold time limit
// has expired to avoid a short popup flash.
//
// State Transition Diagrams:
//
//  T1 - double press time limit (short)
//  T2 - hold to exit time limit (long)
//
//  IDLE
//   | Press
//  WAIT_FOR_QUICK_RELEASE action: show ui & start timers
//   | Release (DT < T1)
//  WAIT_FOR_DOUBLE_PRESS
//   | Press (DT < T1)
//  EXITING action: hide ui, stop timers, exit
//
//  IDLE
//   | Press
//  WAIT_FOR_QUICK_RELEASE action: show ui & start timers
//   | T1 timer expires
//  WAIT_FOR_LONG_HOLD
//   | T2 Timer exipres
//  EXITING action: hide ui, exit
//
//  IDLE
//   | Press
//  WAIT_FOR_QUICK_RELEASE action: show ui & start timers
//   | T1 timer expiers
//  WAIT_FOR_LONG_HOLD
//   | Release
//  CANCELED
//   | T2 timer expires
//  IDLE action: hide ui
//
//  IDLE
//   | Press
//  WAIT_FOR_QUICK_RELEASE action: show ui & start timers
//   | Release (DT < T1)
//  WAIT_FOR_DOUBLE_PRESS
//   | T1 timer expires
//  CANCELED
//   | T2 timer expires
//  IDLE action: hide ui
//

class AcceleratorControllerTest;

class ASH_EXPORT ExitWarningHandler {
 public:
  ExitWarningHandler();

  ~ExitWarningHandler();

  // Handles shortcut key press and release (Ctrl-Shift-Q).
  void HandleExitKey(bool press);

 private:
  friend class AcceleratorControllerTest;

  enum State {
    IDLE,
    WAIT_FOR_QUICK_RELEASE,
    WAIT_FOR_DOUBLE_PRESS,
    WAIT_FOR_LONG_HOLD,
    CANCELED,
    EXITING
  };

  // Performs actions (see state diagram above) when the "double key
  // press" time limit is exceeded. This is the shorter of the two
  // time limits.
  void Timer1Action();

  // Performs actions (see state diagram above) when the hold time
  // limit is exceeded.  See state diagram above. This is the longer
  // of the two time limits.
  void Timer2Action();

  void StartTimers();
  void CancelTimers();

  void Show();
  void Hide();

  State state_;
  views::Widget* widget_; // owned by |this|.
  base::OneShotTimer<ExitWarningHandler> timer1_; // short; double press
  base::OneShotTimer<ExitWarningHandler> timer2_; // long; hold to exit

  // Flag to suppress starting the timers for testing. For test we
  // call TimerAction1() and TimerAction2() directly to simulate the
  // expiration of the timers.
  bool stub_timers_for_test_;

  DISALLOW_COPY_AND_ASSIGN(ExitWarningHandler);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_

