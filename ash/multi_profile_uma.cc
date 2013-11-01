// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_profile_uma.h"

#include "base/metrics/histogram.h"

namespace ash {

// static
void MultiProfileUMA::RecordSessionMode(SessionMode action) {
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.SessionMode",
                            action,
                            NUM_SESSION_MODES);
}

// static
void MultiProfileUMA::RecordSigninUser(SigninUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.SigninUserUIPath",
                            action,
                            NUM_SIGNIN_USER_ACTIONS);
}

// static
void MultiProfileUMA::RecordSwitchActiveUser(SwitchActiveUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.SwitchActiveUserUIPath",
                            action,
                            NUM_SWITCH_ACTIVE_USER_ACTIONS);
}

// static
void MultiProfileUMA::RecordTeleportAction(TeleportWindowAction action) {
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.TeleportWindow",
                            action,
                            NUM_TELEPORT_WINDOW_ACTIONS);
}

}  // namespace ash
