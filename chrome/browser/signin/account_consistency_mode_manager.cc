// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Preference indicating that the Dice migration should happen at the next
// Chrome startup.
const char kDiceMigrationOnStartupPref[] =
    "signin.AccountReconcilor.kDiceMigrationOnStartup";

const char kDiceMigrationStatusHistogram[] = "Signin.DiceMigrationStatus";

// Used for UMA histogram kDiceMigrationStatusHistogram.
// Do not remove or re-order values.
enum class DiceMigrationStatus {
  kEnabled,
  kDisabledReadyForMigration,
  kDisabledNotReadyForMigration,

  // This is the last value. New values should be inserted above.
  kDiceMigrationStatusCount
};
#endif

}  // namespace

AccountConsistencyModeManager::AccountConsistencyModeManager(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool is_ready_for_dice = IsReadyForDiceMigration();
  PrefService* user_prefs = profile->GetPrefs();
  if (is_ready_for_dice && signin::IsDiceMigrationEnabled()) {
    if (!signin::IsDiceEnabledForProfile(user_prefs))
      VLOG(1) << "Profile is migrating to Dice";
    signin::MigrateProfileToDice(user_prefs);
    DCHECK(signin::IsDiceEnabledForProfile(user_prefs));
  }
  UMA_HISTOGRAM_ENUMERATION(
      kDiceMigrationStatusHistogram,
      signin::IsDiceEnabledForProfile(user_prefs)
          ? DiceMigrationStatus::kEnabled
          : (is_ready_for_dice
                 ? DiceMigrationStatus::kDisabledReadyForMigration
                 : DiceMigrationStatus::kDisabledNotReadyForMigration),
      DiceMigrationStatus::kDiceMigrationStatusCount);

#endif
}

AccountConsistencyModeManager::~AccountConsistencyModeManager() {}

// static
void AccountConsistencyModeManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterBooleanPref(kDiceMigrationOnStartupPref, false);
#endif
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountConsistencyModeManager::SetReadyForDiceMigration(bool is_ready) {
  SetDiceMigrationOnStartup(profile_->GetPrefs(), is_ready);
}

// static
void AccountConsistencyModeManager::SetDiceMigrationOnStartup(
    PrefService* prefs,
    bool migrate) {
  VLOG(1) << "Dice migration on next startup: " << migrate;
  prefs->SetBoolean(kDiceMigrationOnStartupPref, migrate);
}

bool AccountConsistencyModeManager::IsReadyForDiceMigration() {
  return profile_->IsNewProfile() ||
         profile_->GetPrefs()->GetBoolean(kDiceMigrationOnStartupPref);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
