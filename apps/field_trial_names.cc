// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/field_trial_names.h"

namespace apps {

// The field trial group name that enables showing the promo.
const char kShowLauncherPromoOnceGroupName[] = "ShowPromoUntilDismissed";

// The field trial group name that resets the pref to show the app launcher
// promo on every session startup.
const char kResetShowLauncherPromoPrefGroupName[] = "ResetShowPromoPref";

// The name of the field trial that controls showing the app launcher promo.
const char kLauncherPromoTrialName[] = "ShowAppLauncherPromo";

}  // namespace apps
