// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_util.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/constants/chromeos_switches.h"

namespace chromeos {

bool IsAccountManagerAvailable(const Profile* const profile) {
  if (!switches::IsAccountManagerEnabled())
    return false;

  // Signin Profile does not have any accounts associated with it.
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return false;

  // LockScreenAppProfile does not link to the user's cryptohome.
  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile))
    return false;

  // Account Manager is unavailable on Guest (Incognito) Sessions.
  if (profile->IsGuestSession() || profile->IsOffTheRecord())
    return false;

  // Account Manager is unavailable on Managed Guest Sessions / Public Sessions.
  if (profiles::IsPublicSession())
    return false;

  // Available in all other cases.
  return true;
}

}  // namespace chromeos
