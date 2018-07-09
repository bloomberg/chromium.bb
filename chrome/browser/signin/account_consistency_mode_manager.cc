// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#include "ui/base/ui_base_features.h"
#endif

#if defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_pref_names.h"
#endif

using signin::AccountConsistencyMethod;

const base::Feature kAccountConsistencyFeature{
    "AccountConsistency", base::FEATURE_ENABLED_BY_DEFAULT};
const char kAccountConsistencyFeatureMethodParameter[] = "method";
const char kAccountConsistencyFeatureMethodMirror[] = "mirror";
const char kAccountConsistencyFeatureMethodDiceFixAuthErrors[] =
    "dice_fix_auth_errors";
const char kAccountConsistencyFeatureMethodDicePrepareMigration[] =
    "dice_prepare_migration_new_endpoint";
const char kAccountConsistencyFeatureMethodDiceMigration[] = "dice_migration";
const char kAccountConsistencyFeatureMethodDice[] = "dice";

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Preference indicating that the Dice migration should happen at the next
// Chrome startup.
const char kDiceMigrationOnStartupPref[] =
    "signin.AccountReconcilor.kDiceMigrationOnStartup2";
// Preference indicating that the Dice migraton has happened.
const char kDiceMigrationCompletePref[] = "signin.DiceMigrationComplete";

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

class AccountConsistencyModeManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static AccountConsistencyModeManagerFactory* GetInstance() {
    return base::Singleton<AccountConsistencyModeManagerFactory>::get();
  }

  static AccountConsistencyModeManager* GetForProfile(Profile* profile) {
    DCHECK(profile);
    return static_cast<AccountConsistencyModeManager*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

 private:
  friend struct base::DefaultSingletonTraits<
      AccountConsistencyModeManagerFactory>;

  AccountConsistencyModeManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "AccountConsistencyModeManager",
            BrowserContextDependencyManager::GetInstance()) {}

  ~AccountConsistencyModeManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    DCHECK(!context->IsOffTheRecord());
    Profile* profile = static_cast<Profile*>(context);
    return new AccountConsistencyModeManager(profile);
  }
};

// Returns the default account consistency for guest profiles.
AccountConsistencyMethod GetMethodForNonRegularProfile() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return AccountConsistencyMethod::kDiceFixAuthErrors;
#else
  return AccountConsistencyMethod::kDisabled;
#endif
}

}  // namespace

bool AccountConsistencyModeManager::ignore_missing_oauth_client_for_testing_ =
    false;

// static
AccountConsistencyModeManager* AccountConsistencyModeManager::GetForProfile(
    Profile* profile) {
  return AccountConsistencyModeManagerFactory::GetForProfile(profile);
}

AccountConsistencyModeManager::AccountConsistencyModeManager(Profile* profile)
    : profile_(profile),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
      account_consistency_initialized_(false) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
  account_consistency_ = ComputeAccountConsistencyMethod(profile_);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool is_ready_for_dice = IsReadyForDiceMigration(profile_);
  if (is_ready_for_dice &&
      signin::DiceMethodGreaterOrEqual(
          account_consistency_, AccountConsistencyMethod::kDiceMigration)) {
    if (account_consistency_ != AccountConsistencyMethod::kDice)
      VLOG(1) << "Profile is migrating to Dice";
    profile_->GetPrefs()->SetBoolean(kDiceMigrationCompletePref, true);
    account_consistency_ = AccountConsistencyMethod::kDice;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kDiceMigrationStatusHistogram,
      account_consistency_ == AccountConsistencyMethod::kDice
          ? DiceMigrationStatus::kEnabled
          : (is_ready_for_dice
                 ? DiceMigrationStatus::kDisabledReadyForMigration
                 : DiceMigrationStatus::kDisabledNotReadyForMigration),
      DiceMigrationStatus::kDiceMigrationStatusCount);
#endif

  DCHECK_EQ(account_consistency_, ComputeAccountConsistencyMethod(profile_));
  account_consistency_initialized_ = true;
}

AccountConsistencyModeManager::~AccountConsistencyModeManager() {}

// static
void AccountConsistencyModeManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterBooleanPref(kDiceMigrationOnStartupPref, false);
  registry->RegisterBooleanPref(kDiceMigrationCompletePref, false);
#endif
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAccountConsistencyMirrorRequired,
                                false);
#endif
}

// static
AccountConsistencyMethod AccountConsistencyModeManager::GetMethodForProfile(
    Profile* profile) {
  if (profile->IsOffTheRecord())
    return GetMethodForNonRegularProfile();

  return AccountConsistencyModeManager::GetForProfile(profile)
      ->GetAccountConsistencyMethod();
}

// static
bool AccountConsistencyModeManager::IsDiceEnabledForProfile(Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kDice;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountConsistencyModeManager::SetReadyForDiceMigration(bool is_ready) {
  DCHECK_EQ(Profile::ProfileType::REGULAR_PROFILE, profile_->GetProfileType());
  SetDiceMigrationOnStartup(profile_->GetPrefs(), is_ready);
}

// static
void AccountConsistencyModeManager::SetDiceMigrationOnStartup(
    PrefService* prefs,
    bool migrate) {
  VLOG(1) << "Dice migration on next startup: " << migrate;
  prefs->SetBoolean(kDiceMigrationOnStartupPref, migrate);
}

// static
bool AccountConsistencyModeManager::IsReadyForDiceMigration(Profile* profile) {
  return (profile->GetProfileType() == Profile::ProfileType::REGULAR_PROFILE) &&
         (profile->IsNewProfile() ||
          profile->GetPrefs()->GetBoolean(kDiceMigrationOnStartupPref));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// static
bool AccountConsistencyModeManager::IsMirrorEnabledForProfile(
    Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kMirror;
}

// static
void AccountConsistencyModeManager::SetIgnoreMissingOAuthClientForTesting() {
  ignore_missing_oauth_client_for_testing_ = true;
}

AccountConsistencyMethod
AccountConsistencyModeManager::GetAccountConsistencyMethod() {
#if defined(OS_CHROMEOS)
  // TODO(https://crbug.com/860671): ChromeOS should use the cached value.
  // Changing the value dynamically is not supported.
  return ComputeAccountConsistencyMethod(profile_);
#else
  // The account consistency method should not change during the lifetime of a
  // profile. We always return the cached value, but still check that it did not
  // change, in order to detect inconsisent states. See https://crbug.com/860471
  CHECK(account_consistency_initialized_);
  CHECK_EQ(ComputeAccountConsistencyMethod(profile_), account_consistency_);
  return account_consistency_;
#endif
}

// static
signin::AccountConsistencyMethod
AccountConsistencyModeManager::ComputeAccountConsistencyMethod(
    Profile* profile) {
  if (profile->GetProfileType() != Profile::ProfileType::REGULAR_PROFILE) {
    DCHECK_EQ(Profile::ProfileType::GUEST_PROFILE, profile->GetProfileType());
    return GetMethodForNonRegularProfile();
  }

#if BUILDFLAG(ENABLE_MIRROR)
  return AccountConsistencyMethod::kMirror;
#endif

  std::string method_value = base::GetFieldTrialParamValueByFeature(
      kAccountConsistencyFeature, kAccountConsistencyFeatureMethodParameter);

#if defined(OS_CHROMEOS)
  return (method_value == kAccountConsistencyFeatureMethodMirror ||
          profile->GetPrefs()->GetBoolean(
              prefs::kAccountConsistencyMirrorRequired))
             ? AccountConsistencyMethod::kMirror
             : AccountConsistencyMethod::kDisabled;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  AccountConsistencyMethod method =
      AccountConsistencyMethod::kDicePrepareMigration;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  if (base::FeatureList::IsEnabled(features::kExperimentalUi))
    method = AccountConsistencyMethod::kDiceMigration;
#endif

  if (method_value == kAccountConsistencyFeatureMethodDiceFixAuthErrors)
    method = AccountConsistencyMethod::kDiceFixAuthErrors;
  else if (method_value == kAccountConsistencyFeatureMethodDicePrepareMigration)
    method = AccountConsistencyMethod::kDicePrepareMigration;
  else if (method_value == kAccountConsistencyFeatureMethodDiceMigration)
    method = AccountConsistencyMethod::kDiceMigration;
  else if (method_value == kAccountConsistencyFeatureMethodDice)
    method = AccountConsistencyMethod::kDice;

  if (method == AccountConsistencyMethod::kDiceFixAuthErrors)
    return method;

  DCHECK(signin::DiceMethodGreaterOrEqual(
      method, AccountConsistencyMethod::kDicePrepareMigration));

  // Legacy supervised users cannot get Dice.
  // TODO(droger): remove this once legacy supervised users are no longer
  // supported.
  if (profile->IsLegacySupervised())
    return AccountConsistencyMethod::kDiceFixAuthErrors;

  bool can_enable_dice_for_build = ignore_missing_oauth_client_for_testing_ ||
                                   google_apis::HasOAuthClientConfigured();
  if (!can_enable_dice_for_build) {
    LOG(WARNING) << "Desktop Identity Consistency cannot be enabled as no "
                    "OAuth client ID and client secret have been configured.";
    return AccountConsistencyMethod::kDiceFixAuthErrors;
  }

  if (method == AccountConsistencyMethod::kDiceMigration &&
      profile->GetPrefs()->GetBoolean(kDiceMigrationCompletePref)) {
    return AccountConsistencyMethod::kDice;
  }

  return method;
#endif

  NOTREACHED();
  return AccountConsistencyMethod::kDisabled;
}
