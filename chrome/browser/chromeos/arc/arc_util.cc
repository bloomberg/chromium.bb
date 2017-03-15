// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
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

  // ARC Kiosk can be enabled even if ARC is not yet supported on the device.
  // In that case IsArcKioskMode() should return true as profile is already
  // created.
  if (!IsArcAvailable() && !(IsArcKioskMode() && IsArcKioskAvailable())) {
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

  // Play Store requires an appropriate application install mechanism. Normal
  // users do this through GAIA, but Kiosk and Active Directory users use
  // different application install mechanism. ARC is not allowed otherwise
  // (e.g. in public sessions). cf) crbug.com/605545
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const bool has_gaia_account = user && user->HasGaiaAccount();
  const bool is_arc_active_directory_user =
      user && user->IsActiveDirectoryUser() &&
      IsArcAllowedForActiveDirectoryUsers();
  if (!has_gaia_account && !is_arc_active_directory_user && !IsArcKioskMode()) {
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

bool IsArcPlayStoreEnabledForProfile(const Profile* profile) {
  return IsArcAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

bool IsArcPlayStoreEnabledPreferenceManagedForProfile(const Profile* profile) {
  if (!IsArcAllowedForProfile(profile)) {
    LOG(DFATAL) << "ARC is not allowed for profile";
    return false;
  }
  return profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled);
}

void SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled) {
  DCHECK(IsArcAllowedForProfile(profile));
  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    VLOG(1) << "Google-Play-Store-enabled pref is managed. Request to "
            << (enabled ? "enable" : "disable") << " Play Store is not stored";
    // Need update ARC session manager manually for managed case in order to
    // keep its state up to date, otherwise it may stuck with enabling
    // request.
    // TODO (khmel): Consider finding the better way handling this.
    ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
    // |arc_session_manager| can be nullptr in unit_tests.
    if (!arc_session_manager)
      return;
    if (enabled)
      arc_session_manager->RequestEnable();
    else
      arc_session_manager->RequestDisable();
    return;
  }
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, enabled);
}

bool AreArcAllOptInPreferencesManagedForProfile(const Profile* profile) {
  return profile->GetPrefs()->IsManagedPreference(
             prefs::kArcBackupRestoreEnabled) &&
         profile->GetPrefs()->IsManagedPreference(
             prefs::kArcLocationServiceEnabled);
}

}  // namespace arc
