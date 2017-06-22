// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include <linux/magic.h>
#include <sys/statfs.h>
#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

using DeviceEcryptfsMigrationStrategyProto =
    enterprise_management::DeviceEcryptfsMigrationStrategyProto;

namespace arc {

namespace {

constexpr char kLsbReleaseArcVersionKey[] = "CHROMEOS_ARC_ANDROID_SDK_VERSION";
constexpr char kAndroidMSdkVersion[] = "23";

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

// This flag is set the first time the check if migration to ext4 is allowed,
// and remains unchanged after that.
// TODO(igorcov): Remove this after migration. crbug.com/725493
base::Optional<bool> g_is_arc_migration_allowed;

// Returns whether ARC can run on the filesystem mounted at |path|.
// This function should run only on threads where IO operations are allowed.
bool IsArcCompatibleFilesystem(const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

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

bool IsArcMigrationAllowedInternal() {
  // If the device is not managed, then the migration allowed.
  if (!g_browser_process->platform_part()
           ->browser_policy_connector_chromeos()
           ->IsEnterpriseManaged()) {
    return true;
  }

  const auto* const command_line = base::CommandLine::ForCurrentProcess();
  // If the command line flag is missing, the migration for this type of
  // device is allowed regardless of the policy data.
  if (!command_line->HasSwitch(
          chromeos::switches::kNeedArcMigrationPolicyCheck)) {
    return true;
  }

  const auto* policy =
      chromeos::DeviceSettingsService::Get()->device_settings();
  if (policy && policy->has_device_ecryptfs_migration_strategy()) {
    const DeviceEcryptfsMigrationStrategyProto& container(
        policy->device_ecryptfs_migration_strategy());
    return container.has_migration_strategy() &&
           container.migration_strategy() ==
               DeviceEcryptfsMigrationStrategyProto::ALLOW_MIGRATION;
  }

  return false;
}

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
  if (!IsArcAllowedForUser(user)) {
    VLOG(1) << "ARC is not allowed for the user.";
    return false;
  }

  // If migration policy check is needed (specified by commandline flag), check
  // the policy, which should be already available here. If policy says
  // migration is not allowed, do not run ARC, regardless whether file system
  // migration is actually needed. For example, even if file system is still
  // ecryptfs and ARC version is M, or file system is already migrated into ext4
  // crypt and ARC version is N or later, if policy says migration is not
  // allowed, ARC will never run. Practically, in the former example case,
  // --need-arc-migration-policy-check is not set, so this check passes and user
  // can use ARC. In latter case, policy should say migration is allowed, so
  // also user can use ARC then.
  // TODO(igorcov): Remove this after migration. crbug.com/725493
  if (!IsArcMigrationAllowed()) {
    VLOG(1) << "ARC migration is not allowed by policy.";
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

  return true;
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
    // TODO (khmel): Consider finding the better way handling this.
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

bool AreArcAllOptInPreferencesManagedForProfile(const Profile* profile) {
  return profile->GetPrefs()->IsManagedPreference(
             prefs::kArcBackupRestoreEnabled) &&
         profile->GetPrefs()->IsManagedPreference(
             prefs::kArcLocationServiceEnabled);
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

bool IsArcMigrationAllowed() {
  if (!g_is_arc_migration_allowed.has_value())
    g_is_arc_migration_allowed = IsArcMigrationAllowedInternal();
  return g_is_arc_migration_allowed.value();
}

void ResetArcMigrationAllowedForTesting() {
  g_is_arc_migration_allowed.reset();
}

}  // namespace arc
