// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"

namespace signin {

namespace {

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

}  // namespace

DiceAccountReconcilorDelegate::DiceAccountReconcilorDelegate(
    PrefService* user_prefs,
    bool is_new_profile)
    : user_prefs_(user_prefs) {
  DCHECK(user_prefs_);
  bool is_ready_for_dice = IsReadyForDiceMigration(is_new_profile);
  if (is_ready_for_dice && IsDiceMigrationEnabled()) {
    if (!IsDiceEnabledForProfile(user_prefs_))
      VLOG(1) << "Profile is migrating to Dice";
    MigrateProfileToDice(user_prefs_);
    DCHECK(IsDiceEnabledForProfile(user_prefs_));
  }
  UMA_HISTOGRAM_ENUMERATION(
      kDiceMigrationStatusHistogram,
      IsDiceEnabledForProfile(user_prefs_)
          ? DiceMigrationStatus::kEnabled
          : (is_ready_for_dice
                 ? DiceMigrationStatus::kDisabledReadyForMigration
                 : DiceMigrationStatus::kDisabledNotReadyForMigration),
      DiceMigrationStatus::kDiceMigrationStatusCount);
}

// static
void DiceAccountReconcilorDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kDiceMigrationOnStartupPref, false);
}

// static
void DiceAccountReconcilorDelegate::SetDiceMigrationOnStartup(
    PrefService* prefs,
    bool migrate) {
  VLOG(1) << "Dice migration on next startup: " << migrate;
  prefs->SetBoolean(kDiceMigrationOnStartupPref, migrate);
}

bool DiceAccountReconcilorDelegate::IsReadyForDiceMigration(
    bool is_new_profile) {
  return is_new_profile || user_prefs_->GetBoolean(kDiceMigrationOnStartupPref);
}

bool DiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return IsDicePrepareMigrationEnabled();
}

bool DiceAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return IsDiceEnabledForProfile(user_prefs_);
}

// - On first execution, the candidates are examined in this order:
//   1. The primary account
//   2. The current first Gaia account
//   3. The last known first Gaia account
//   4. The first account in the token service
// - On subsequent executions, the order is:
//   1. The current first Gaia account
//   2. The primary account
//   3. The last known first Gaia account
//   4. The first account in the token service
std::string DiceAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution) const {
  if (chrome_accounts.empty())
    return std::string();  // No Chrome account, log out.

  bool valid_primary_account =
      !primary_account.empty() &&
      base::ContainsValue(chrome_accounts, primary_account);

  if (gaia_accounts.empty()) {
    if (valid_primary_account)
      return primary_account;

    // Try the last known account. This happens when the cookies are cleared
    // while Sync is disabled.
    if (base::ContainsValue(chrome_accounts, last_known_first_account_))
      return last_known_first_account_;

    // As a last resort, use the first Chrome account.
    return chrome_accounts[0];
  }

  const std::string& first_gaia_account = gaia_accounts[0].id;
  bool first_gaia_account_is_valid =
      gaia_accounts[0].valid &&
      base::ContainsValue(chrome_accounts, first_gaia_account);

  if (!first_gaia_account_is_valid && (primary_account == first_gaia_account)) {
    // The primary account is also the first Gaia account, and is invalid.
    // Logout everything.
    return std::string();
  }

  if (first_execution) {
    // On first execution, try the primary account, and then the first Gaia
    // account.
    if (valid_primary_account)
      return primary_account;
    if (first_gaia_account_is_valid)
      return first_gaia_account;
    // As a last resort, use the first Chrome account.
    return chrome_accounts[0];
  }

  // While Chrome is running, try the first Gaia account, and then the
  // primary account.
  if (first_gaia_account_is_valid)
    return first_gaia_account;
  if (valid_primary_account)
    return primary_account;

  // Changing the first Gaia account while Chrome is running would be
  // confusing for the user. Logout everything.
  return std::string();
}

void DiceAccountReconcilorDelegate::OnReconcileFinished(
    const std::string& first_account,
    bool reconcile_is_noop) {
  last_known_first_account_ = first_account;

  // Migration happens on startup if the last reconcile was a no-op.
  if (IsDicePrepareMigrationEnabled())
    SetDiceMigrationOnStartup(user_prefs_, reconcile_is_noop);
}

}  // namespace signin
