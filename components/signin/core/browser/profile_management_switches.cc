// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_management_switches.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_features.h"
#include "components/signin/core/browser/signin_switches.h"

#if defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_pref_names.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/prefs/pref_service.h"
#endif

namespace signin {

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kDiceMigrationCompletePref[] = "signin.DiceMigrationComplete";

// Returns whether Dice is enabled for the user, based on the account
// consistency mode and the dice pref value.
bool IsDiceEnabledForPrefValue(bool dice_pref_value) {
  switch (GetAccountConsistencyMethod()) {
    case AccountConsistencyMethod::kDisabled:
    case AccountConsistencyMethod::kMirror:
    case AccountConsistencyMethod::kDiceFixAuthErrors:
    case AccountConsistencyMethod::kDicePrepareMigration:
    case AccountConsistencyMethod::kDicePrepareMigrationChromeSyncEndpoint:
      return false;
    case AccountConsistencyMethod::kDice:
      return true;
    case AccountConsistencyMethod::kDiceMigration:
      return dice_pref_value;
  }
  NOTREACHED();
  return false;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Returns a callback telling if site isolation is enabled for Gaia origins.
base::RepeatingCallback<bool()>* GetIsGaiaIsolatedCallback() {
  static base::RepeatingCallback<bool()> g_is_gaia_isolated_callback;
  return &g_is_gaia_isolated_callback;
}

bool AccountConsistencyMethodGreaterOrEqual(AccountConsistencyMethod a,
                                            AccountConsistencyMethod b) {
  return static_cast<int>(a) >= static_cast<int>(b);
}

}  // namespace

// base::Feature definitions.
const base::Feature kAccountConsistencyFeature{
    "AccountConsistency", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAccountConsistencyFeatureMethodParameter[] = "method";
const char kAccountConsistencyFeatureMethodMirror[] = "mirror";
const char kAccountConsistencyFeatureMethodDiceFixAuthErrors[] =
    "dice_fix_auth_errors";
const char kAccountConsistencyFeatureMethodDicePrepareMigration[] =
    "dice_prepare_migration";
const char
    kAccountConsistencyFeatureMethodDicePrepareMigrationChromeSyncEndpoint[] =
        "dice_prepare_migration_new_endpoint";
const char kAccountConsistencyFeatureMethodDiceMigration[] = "dice_migration";
const char kAccountConsistencyFeatureMethodDice[] = "dice";

void RegisterAccountConsistencyProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAccountConsistencyMirrorRequired,
                                false);
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterBooleanPref(kDiceMigrationCompletePref, false);
#endif
}

AccountConsistencyMethod GetAccountConsistencyMethod() {
#if BUILDFLAG(ENABLE_MIRROR)
  // Mirror is always enabled on Android and iOS.
  return AccountConsistencyMethod::kMirror;
#else

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(!GetIsGaiaIsolatedCallback()->is_null());
  if (!GetIsGaiaIsolatedCallback()->Run()) {
    // Because of limitations in base::Feature, always return kDisabled when
    // Gaia is not isolated, even though it's not technically a requirement for
    // all account consistency methods (i.e. kDiceFixAuthErrors could be
    // allowed).
    return AccountConsistencyMethod::kDisabled;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  if (!base::FeatureList::IsEnabled(kAccountConsistencyFeature))
    return AccountConsistencyMethod::kDisabled;

  std::string method_value = base::GetFieldTrialParamValueByFeature(
      kAccountConsistencyFeature, kAccountConsistencyFeatureMethodParameter);

  if (method_value == kAccountConsistencyFeatureMethodMirror)
    return AccountConsistencyMethod::kMirror;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  else if (method_value == kAccountConsistencyFeatureMethodDiceFixAuthErrors)
    return AccountConsistencyMethod::kDiceFixAuthErrors;
  else if (method_value == kAccountConsistencyFeatureMethodDicePrepareMigration)
    return AccountConsistencyMethod::kDicePrepareMigration;
  else if (
      method_value ==
      kAccountConsistencyFeatureMethodDicePrepareMigrationChromeSyncEndpoint) {
    return AccountConsistencyMethod::kDicePrepareMigrationChromeSyncEndpoint;
  } else if (method_value == kAccountConsistencyFeatureMethodDiceMigration)
    return AccountConsistencyMethod::kDiceMigration;
  else if (method_value == kAccountConsistencyFeatureMethodDice)
    return AccountConsistencyMethod::kDice;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  return AccountConsistencyMethod::kDisabled;
#endif  // BUILDFLAG(ENABLE_MIRROR)
}

bool IsAccountConsistencyMirrorEnabled() {
  return GetAccountConsistencyMethod() == AccountConsistencyMethod::kMirror;
}

bool IsDicePrepareMigrationEnabled() {
  return AccountConsistencyMethodGreaterOrEqual(
      GetAccountConsistencyMethod(),
      AccountConsistencyMethod::kDicePrepareMigration);
}

bool IsDicePrepareMigrationChromeSyncEndpointEnabled() {
  return AccountConsistencyMethodGreaterOrEqual(
      GetAccountConsistencyMethod(),
      AccountConsistencyMethod::kDicePrepareMigrationChromeSyncEndpoint);
}

bool IsDiceMigrationEnabled() {
  return AccountConsistencyMethodGreaterOrEqual(
      GetAccountConsistencyMethod(), AccountConsistencyMethod::kDiceMigration);
}

bool IsDiceEnabledForProfile(const PrefService* user_prefs) {
  DCHECK(user_prefs);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return IsDiceEnabledForPrefValue(
      user_prefs->GetBoolean(kDiceMigrationCompletePref));
#else
  return false;
#endif
}

bool IsDiceEnabled(const BooleanPrefMember* dice_pref_member) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(dice_pref_member);
  DCHECK_EQ(kDiceMigrationCompletePref, dice_pref_member->GetPrefName());
  return IsDiceEnabledForPrefValue(dice_pref_member->GetValue());
#else
  return false;
#endif
}

std::unique_ptr<BooleanPrefMember> CreateDicePrefMember(
    PrefService* user_prefs) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<BooleanPrefMember> pref_member =
      std::make_unique<BooleanPrefMember>();
  pref_member->Init(kDiceMigrationCompletePref, user_prefs);
  return pref_member;
#else
  return std::unique_ptr<BooleanPrefMember>();
#endif
}

void MigrateProfileToDice(PrefService* user_prefs) {
  DCHECK(IsDiceMigrationEnabled());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  user_prefs->SetBoolean(kDiceMigrationCompletePref, true);
#endif
}

bool IsDiceFixAuthErrorsEnabled() {
  return AccountConsistencyMethodGreaterOrEqual(
      GetAccountConsistencyMethod(),
      AccountConsistencyMethod::kDiceFixAuthErrors);
}

bool IsExtensionsMultiAccount() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  NOTREACHED() << "Extensions are not enabled on Android or iOS";
  // Account consistency is enabled on Android and iOS.
  return false;
#endif

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kExtensionsMultiAccount) ||
         GetAccountConsistencyMethod() == AccountConsistencyMethod::kMirror;
}

void SetGaiaOriginIsolatedCallback(
    const base::RepeatingCallback<bool()>& is_gaia_isolated) {
  *GetIsGaiaIsolatedCallback() = is_gaia_isolated;
}

}  // namespace signin
