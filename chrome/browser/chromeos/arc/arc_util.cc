// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace arc {

namespace {

// Let IsAllowedForProfile() return "false" for any profile.
bool g_disallow_for_testing = false;

}  // namespace

bool IsArcAllowedForProfile(const Profile* profile) {
  if (g_disallow_for_testing) {
    VLOG(1) << "ARC is disallowed for testing.";
    return false;
  }

  if (!IsArcAvailable()) {
    VLOG(1) << "ARC is not available.";
    return false;
  }

  if (!profile) {
    VLOG(1) << "ARC is not supported for systems without profile.";
    return false;
  }

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG(1) << "Non-primary users are not supported in ARC.";
    return false;
  }

  // IsPrimaryProfile can return true for an incognito profile corresponding
  // to the primary profile, but ARC does not support it.
  if (profile->IsOffTheRecord()) {
    VLOG(1) << "Incognito profile is not supported in ARC.";
    return false;
  }

  if (profile->IsLegacySupervised()) {
    VLOG(1) << "Supervised users are not supported in ARC.";
    return false;
  }

  // Do not allow for public session. Communicating with Play Store requires
  // GAIA account. An exception is Kiosk mode, which uses different application
  // install mechanism. cf) crbug.com/605545
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if ((!user || !user->HasGaiaAccount()) && !IsArcKioskMode()) {
    VLOG(1) << "Users without GAIA accounts are not supported in ARC.";
    return false;
  }

  // Do not run ARC instance when supervised user is being created.
  // Otherwise noisy notification may be displayed.
  chromeos::UserFlow* user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user->GetAccountId());
  if (!user_flow || !user_flow->CanStartArc()) {
    VLOG(1) << "ARC is not allowed in the current user flow.";
    return false;
  }

  // Do not allow for Ephemeral data user. cf) b/26402681
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    VLOG(1) << "Users with ephemeral data are not supported in ARC.";
    return false;
  }

  return true;
}

void DisallowArcForTesting() {
  g_disallow_for_testing = true;
}

}  // namespace arc
