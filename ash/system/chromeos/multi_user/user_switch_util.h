// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_MULTI_USER_MULTI_USER_SWITCH_UTIL_H_
#define ASH_SYSTEM_CHROMEOS_MULTI_USER_MULTI_USER_SWITCH_UTIL_H_

#include "ash/ash_export.h"
#include "base/callback.h"

namespace ash {

// Tries to switch to a new user by first checking if desktop casting / sharing
// is going on, and let the user decide if he wants to terminate it or not.
// After terminating any desktop sharing operations, the |switch_user| function
// will be called.
void ASH_EXPORT TrySwitchingActiveUser(
    const base::Callback<void()> switch_user);

// Terminates the "DesktopCastingWarning" dialog from a unit tests and |accept|s
// it. False will be returned if there was no dialog shown.
bool ASH_EXPORT TestAndTerminateDesktopCastingWarningForTest(bool accept);

}  // namespace chromeos

#endif  // ASH_SYSTEM_CHROMEOS_MULTI_USER_MULTI_USER_SWITCH_UTIL_H_
