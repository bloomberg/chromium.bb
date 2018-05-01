// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_PROFILE_UMA_H_
#define ASH_MULTI_PROFILE_UMA_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// Records UMA statistics for multiprofile actions.
// Note: There is also an action to switch profile windows from the
// browser frame that is recorded by the "Profile.OpenMethod" metric.
class ASH_EXPORT MultiProfileUMA {
 public:
  // Used for UMA metrics. Do not reorder.
  enum SigninUserAction {
    SIGNIN_USER_BY_TRAY = 0,
    SIGNIN_USER_BY_BROWSER_FRAME,  // Deprecated.
    NUM_SIGNIN_USER_ACTIONS
  };

  // Used for UMA metrics. Do not reorder.
  enum SwitchActiveUserAction {
    SWITCH_ACTIVE_USER_BY_TRAY = 0,
    SWITCH_ACTIVE_USER_BY_ACCELERATOR,
    NUM_SWITCH_ACTIVE_USER_ACTIONS
  };

  // Record signing in a new user and what UI path was taken.
  static void RecordSigninUser(SigninUserAction action);

  // Record switching the active user and what UI path was taken.
  static void RecordSwitchActiveUser(SwitchActiveUserAction action);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MultiProfileUMA);
};

}  // namespace ash

#endif  // ASH_MULTI_PROFILE_UMA_H_
