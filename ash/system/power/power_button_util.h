// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_UTIL_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_UTIL_H_

#include "base/time/time.h"

namespace ash {
class LockStateController;
class SessionController;
}  // namespace ash

namespace power_button_util {

// Ignore button-up events occurring within this many milliseconds of the
// previous button-up event. This prevents us from falling behind if the power
// button is pressed repeatedly.
static constexpr base::TimeDelta kIgnoreRepeatedButtonUpDelay =
    base::TimeDelta::FromMilliseconds(500);

// Amount of time since last screen state change that power button event needs
// to be ignored.
static constexpr base::TimeDelta kScreenStateChangeDelay =
    base::TimeDelta::FromMilliseconds(500);

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
static constexpr base::TimeDelta kIgnorePowerButtonAfterResumeDelay =
    base::TimeDelta::FromSeconds(2);

// Locks the screen if the "Show lock screen when waking from sleep" pref is set
// and locking is possible.
void LockScreenIfRequired(ash::SessionController* session_controller,
                          ash::LockStateController* lock_state_controller);

}  // namespace power_button_util

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_UTIL_H_
