// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include <linux/magic.h>
#include <sys/statfs.h>
#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace arc {

namespace {

constexpr char kLsbReleaseArcVersionKey[] = "CHROMEOS_ARC_ANDROID_SDK_VERSION";
constexpr char kAndroidMSdkVersion[] = "23";

// Contains set of profiles for which decline reson was already reported.
base::LazyInstance<std::set<base::FilePath>>::DestructorAtExit
    g_profile_declined_set = LAZY_INSTANCE_INITIALIZER;

// The cached value of migration allowed for profile. It is necessary to use
// the same value during a user session.
base::LazyInstance<std::map<base::FilePath, bool>>::DestructorAtExit
    g_is_arc_migration_allowed = LAZY_INSTANCE_INITIALIZER;

// Let IsAllowedForProfile() return "false" for any profile.
bool g_disallow_for_testing = false;

// Let IsArcBlockedDueToIncompatibleFileSystem() return the specified value
// during test runs.
bool g_arc_blocked_due_to_incomaptible_filesystem_for_testing = false;

// TODO(kinaba): Temporary workaround for crbug.com/729034.
//
// Some type of accounts don't have user prefs. As a short-term workaround,
// store the compatibility info from them on memory, ignoring the defect that
// it cannot survive browser crash and restart.
//
// This will be removed once the forced migration for ARC Kiosk user is
// implemented. After it's done such types of accounts cannot even sign-in
// with incompatible filesystem. Hence it'll be safe to always regard compatible
// for them then.
base::LazyInstance<std::set<AccountId>>::DestructorAtExit
    g_known_compatible_users = LAZY_INSTANCE_INITIALIZER;

// Returns whether ARC can run on the filesystem mounted at |path|.
// This function should run only on threads where IO operations are allowed.
bool IsArcCompatibleFilesystem(const base::FilePath& path) {
  base::AssertBlockingAllowed();

  // If it can be verified it is not on ecryptfs, then it is ok.
  struct statfs statfs_buf;
  if (statfs(path.value().c_str(), &statfs_buf) < 0)
    return false;
  return statfs_buf.f_type != ECRYPTFS_SUPER_MAGIC;
}

FileSystemCompatibilityState GetFileSystemCompatibilityPref(
    const AccountId& account_id) {
  int pref_value = kFileSystemIncompatible;
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen, &pref_value);
  return static_cast<FileSystemCompatibilityState>(pref_value);
}

// Stores the result of IsArcCompatibleFilesystem posted back from the blocking
// task runner.
void StoreCompatibilityCheckResult(const AccountId& account_id,
                                   const base::Closure& callback,
                                   bool is_compatible) {
  if (is_compatible) {
    user_manager::known_user::SetIntegerPref(
        account_id, prefs::kArcCompatibleFilesystemChosen,
        arc::kFileSystemCompatible);

    // TODO(kinaba): Remove this code for accounts without user prefs.
    // See the comment for |g_known_compatible_users| for the detail.
    if (GetFileSystemCompatibilityPref(account_id) !=
        arc::kFileSystemCompatible) {
      g_known_compatible_users.Get().insert(account_id);
    }
  }
  callback.Run();
}

// Returns true if this is called first time for profile. Used to detect
// duplicate message for the same profile.
bool IsReportingFirstTimeForProfile(const Profile* profile) {
  const base::FilePath path = profile->GetPath();
  bool inserted;
  std::tie(std::ignore, inserted) = g_profile_declined_set.Get().insert(path);
  return inserted;
}

bool IsArcMigrationAllowedInternal(const Profile* profile) {
  policy_util::EcryptfsMigrationAction migration_strategy =
      policy_util::GetDefaultEcryptfsMigrationActionForManagedUser(
          IsActiveDirectoryUserForProfile(profile));
  if (profile->GetPrefs()->IsManagedPreference(
          prefs::kEcryptfsMigrationStrategy)) {
    migration_strategy = static_cast<policy_util::EcryptfsMigrationAction>(
        profile->GetPrefs()->GetInteger(prefs::kEcryptfsMigrationStrategy));
  }
  // |kAskForEcryptfsArcUsers| value is received only if the device is in EDU
  // and admin left the migration policy unset. Note that when enabling ARC on
  // the admin console, it is mandatory for the administrator to also choose a
  // migration policy.
  // In this default case, only a group of devices that had ARC M enabled are
  // allowed to migrate, provided that ARC is enabled by policy.
  // TODO(pmarko): Remove the special kAskForEcryptfsArcUsers handling when we
  // assess that it's not necessary anymore: crbug.com/761348.
  if (migration_strategy ==
      policy_util::EcryptfsMigrationAction::kAskForEcryptfsArcUsers) {
    // Note that ARC enablement is controlled by policy for managed users (as
    // it's marked 'default_for_enterprise_users': False in
    // policy_templates.json).
    DCHECK(profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled));
    // We can't reuse IsArcPlayStoreEnabledForProfile here because this would
    // lead to a circular dependency: It ends up calling this function for some
    // cases.
    return profile->GetPrefs()->GetBoolean(prefs::kArcEnabled) &&
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               chromeos::switches::kArcTransitionMigrationRequired);
  }

  return migration_strategy !=
         policy_util::EcryptfsMigrationAction::kDisallowMigration;
}

}  // namespace

bool IsArcAllowedForProfile(const Profile* profile) {
  // Silently ignore default and lock screen profiles.
  if (!profile || chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }

  if (g_disallow_for_testing) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "ARC is disallowed for testing.";
    return false;
  }

  // ARC Kiosk can be enabled even if ARC is not yet supported on the device.
  // In that case IsArcKioskMode() should return true as profile is already
  // created.
  if (!IsArcAvailable() && !(IsArcKioskMode() && IsArcKioskAvailable())) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "ARC is not available.";
    return false;
  }

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "Non-primary users are not supported in ARC.";
    return false;
  }

  // IsPrimaryProfile can return true for an incognito profile corresponding
  // to the primary profile, but ARC does not support it.
  if (profile->IsOffTheRecord()) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "Incognito profile is not supported in ARC.";
    return false;
  }

  if (profile->IsLegacySupervised()) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "Supervised users are not supported in ARC.";
    return false;
  }

  if (IsArcBlockedDueToIncompatibleFileSystem(profile) &&
      !IsArcMigrationAllowedByPolicyForProfile(profile)) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "Incompatible encryption and migration forbidden.";
    return false;
  }

  // Play Store requires an appropriate application install mechanism. Normal
  // users do this through GAIA, but Kiosk and Active Directory users use
  // different application install mechanism. ARC is not allowed otherwise
  // (e.g. in public sessions). cf) crbug.com/605545
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsArcAllowedForUser(user)) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "ARC is not allowed for the user.";
    return false;
  }

  // Do not run ARC instance when supervised user is being created.
  // Otherwise noisy notification may be displayed.
  chromeos::UserFlow* user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user->GetAccountId());
  if (!user_flow || !user_flow->CanStartArc()) {
    VLOG_IF(1, IsReportingFirstTimeForProfile(profile))
        << "ARC is not allowed in the current user flow.";
    return false;
  }

  if (chromeos::UserSessionManager::NeedRestartToApplyPerSessionFlagsForProfile(
          profile)) {
    // Quickly restarting ARC instance can cause black screen. crbug.com/758820.
    VLOG(1) << "Do not start ARC because chrome will restart";
    return false;
  }

  return true;
}

bool IsArcMigrationAllowedByPolicyForProfile(const Profile* profile) {
  // Always allow migration for unmanaged users.
  // We're checking if kArcEnabled is managed to find out if the profile is
  // managed. This is equivalent, because kArcEnabled is marked
  // 'default_for_enterprise_users': False in policy_templates.json).
  // Also note that IsArcPlayStoreEnabledPreferenceManagedForProfile cannot be
  // used here due to function call chain (it calls IsArcAllowedForProfile
  // again).
  // TODO(pmarko): crbug.com/771666: Figure out a nicer way to do this on a
  // const Profile*.
  if (!profile ||
      !profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled)) {
    return true;
  }

  // Use the profile path as unique identifier for profile.
  const base::FilePath path = profile->GetPath();
  auto iter = g_is_arc_migration_allowed.Get().find(path);
  if (iter == g_is_arc_migration_allowed.Get().end()) {
    iter = g_is_arc_migration_allowed.Get()
               .emplace(path, IsArcMigrationAllowedInternal(profile))
               .first;
  }

  return iter->second;
}

bool IsArcBlockedDueToIncompatibleFileSystem(const Profile* profile) {
  // Test runs on Linux workstation does not have expected /etc/lsb-release
  // field nor profile creation step. Hence it returns a dummy test value.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return g_arc_blocked_due_to_incomaptible_filesystem_for_testing;

  // Conducts the actual check, only when running on a real Chrome OS device.
  return !IsArcCompatibleFileSystemUsedForProfile(profile);
}

void SetArcBlockedDueToIncompatibleFileSystemForTesting(bool block) {
  g_arc_blocked_due_to_incomaptible_filesystem_for_testing = block;
}

bool IsArcCompatibleFileSystemUsedForProfile(const Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);

  // Returns false for profiles not associated with users (like sign-in profile)
  if (!user)
    return false;

  // chromeos::UserSessionManager does the actual file system check and stores
  // the result to prefs, so that it survives crash-restart.
  FileSystemCompatibilityState filesystem_compatibility =
      GetFileSystemCompatibilityPref(user->GetAccountId());
  const bool is_filesystem_compatible =
      filesystem_compatibility != kFileSystemIncompatible ||
      g_known_compatible_users.Get().count(user->GetAccountId()) != 0;
  std::string arc_sdk_version;
  const bool is_M = base::SysInfo::GetLsbReleaseValue(kLsbReleaseArcVersionKey,
                                                      &arc_sdk_version) &&
                    arc_sdk_version == kAndroidMSdkVersion;

  // To run ARC we want to make sure either
  // - Underlying file system is compatible with ARC, or
  // - SDK version is M.
  if (!is_filesystem_compatible && !is_M) {
    VLOG(1)
        << "Users with SDK version (" << arc_sdk_version
        << ") are not supported when they postponed to migrate to dircrypto.";
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

bool SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled) {
  DCHECK(IsArcAllowedForProfile(profile));
  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    if (enabled && !IsArcPlayStoreEnabledForProfile(profile)) {
      LOG(WARNING) << "Attempt to enable disabled by policy ARC.";
      return false;
    }
    VLOG(1) << "Google-Play-Store-enabled pref is managed. Request to "
            << (enabled ? "enable" : "disable") << " Play Store is not stored";
    // Need update ARC session manager manually for managed case in order to
    // keep its state up to date, otherwise it may stuck with enabling
    // request.
    // TODO(khmel): Consider finding the better way handling this.
    ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
    // |arc_session_manager| can be nullptr in unit_tests.
    if (!arc_session_manager)
      return false;
    if (enabled)
      arc_session_manager->RequestEnable();
    else
      arc_session_manager->RequestDisable();
    return true;
  }
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, enabled);
  return true;
}

bool AreArcAllOptInPreferencesIgnorableForProfile(const Profile* profile) {
  // For Active Directory users, a LaForge account is created, where
  // backup&restore and location services are not supported, hence the policies
  // are unused.
  if (IsActiveDirectoryUserForProfile(profile))
    return true;

  if (profile->GetPrefs()->IsManagedPreference(
          prefs::kArcBackupRestoreEnabled) &&
      profile->GetPrefs()->IsManagedPreference(
          prefs::kArcLocationServiceEnabled)) {
    return true;
  }

  return false;
}

bool IsActiveDirectoryUserForProfile(const Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user ? user->IsActiveDirectoryUser() : false;
}

void UpdateArcFileSystemCompatibilityPrefIfNeeded(
    const AccountId& account_id,
    const base::FilePath& profile_path,
    const base::Closure& callback) {
  DCHECK(!callback.is_null());

  // If ARC is not available, skip the check.
  // This shortcut is just for merginally improving the log-in performance on
  // old devices without ARC. We can always safely remove the following 4 lines
  // without changing any functionality when, say, the code clarity becomes
  // more important in the future.
  if (!IsArcAvailable() && !IsArcKioskAvailable()) {
    callback.Run();
    return;
  }

  // If the compatibility has been already confirmed, skip the check.
  if (GetFileSystemCompatibilityPref(account_id) != kFileSystemIncompatible) {
    callback.Run();
    return;
  }

  // Otherwise, check the underlying filesystem.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&IsArcCompatibleFilesystem, profile_path),
      base::Bind(&StoreCompatibilityCheckResult, account_id, callback));
}

}  // namespace arc
