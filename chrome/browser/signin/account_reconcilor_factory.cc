// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/core/browser/signin_buildflags.h"

#if defined(OS_CHROMEOS)
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"
#include "chrome/browser/chromeos/account_manager/account_migration_runner.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#endif

#if defined(OS_ANDROID)
#include "components/signin/core/browser/mice_account_reconcilor_delegate.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
class ChromeOSChildAccountReconcilorDelegate
    : public signin::MirrorAccountReconcilorDelegate {
 public:
  explicit ChromeOSChildAccountReconcilorDelegate(
      identity::IdentityManager* identity_manager)
      : signin::MirrorAccountReconcilorDelegate(identity_manager) {}

  base::TimeDelta GetReconcileTimeout() const override {
    return base::TimeDelta::FromSeconds(10);
  }

  void OnReconcileError(const GoogleServiceAuthError& error) override {
    // If |error| is |GoogleServiceAuthError::State::NONE| or a transient error.
    if (!error.IsPersistentError()) {
      return;
    }

    // Mark the account to require an online sign in.
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    user_manager::UserManager::Get()->SaveForceOnlineSignin(
        primary_user->GetAccountId(), true /* force_online_signin */);

    // Force a logout.
    UMA_HISTOGRAM_BOOLEAN(
        "ChildAccountReconcilor.ForcedUserExitOnReconcileError", true);
    chrome::AttemptUserExit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOSChildAccountReconcilorDelegate);
};

// An |AccountReconcilorDelegate| for Chrome OS that is exactly the same as
// |MirrorAccountReconcilorDelegate|, except that it does not begin account
// reconciliation until accounts have been migrated to Chrome OS Account
// Manager.
// TODO(sinhak): Remove this when all users have been migrated to Chrome OS
// Account Manager.
class ChromeOSAccountReconcilorDelegate
    : public signin::MirrorAccountReconcilorDelegate {
 public:
  ChromeOSAccountReconcilorDelegate(
      identity::IdentityManager* identity_manager,
      chromeos::AccountManagerMigrator* account_migrator)
      : signin::MirrorAccountReconcilorDelegate(identity_manager),
        account_migrator_(account_migrator) {}
  ~ChromeOSAccountReconcilorDelegate() override = default;

 private:
  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override {
    if (!MirrorAccountReconcilorDelegate::IsReconcileEnabled()) {
      return false;
    }

    const chromeos::AccountMigrationRunner::Status status =
        account_migrator_->GetStatus();
    return status != chromeos::AccountMigrationRunner::Status::kNotStarted &&
           status != chromeos::AccountMigrationRunner::Status::kRunning;
  }

  // A non-owning pointer.
  const chromeos::AccountManagerMigrator* const account_migrator_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSAccountReconcilorDelegate);
};
#endif  // defined(OS_CHROMEOS)

}  // namespace

AccountReconcilorFactory::AccountReconcilorFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountReconcilor",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountReconcilorFactory::~AccountReconcilorFactory() {}

// static
AccountReconcilor* AccountReconcilorFactory::GetForProfile(Profile* profile) {
  return static_cast<AccountReconcilor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountReconcilorFactory* AccountReconcilorFactory::GetInstance() {
  return base::Singleton<AccountReconcilorFactory>::get();
}

KeyedService* AccountReconcilorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountReconcilor* reconcilor = new AccountReconcilor(
      IdentityManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      CreateAccountReconcilorDelegate(profile));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

// static
std::unique_ptr<signin::AccountReconcilorDelegate>
AccountReconcilorFactory::CreateAccountReconcilorDelegate(Profile* profile) {
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  switch (account_consistency) {
    case signin::AccountConsistencyMethod::kMirror:
#if defined(OS_CHROMEOS)
      // Only for child accounts on Chrome OS, use the specialized Mirror
      // delegate.
      if (profile->IsChild()) {
        return std::make_unique<ChromeOSChildAccountReconcilorDelegate>(
            IdentityManagerFactory::GetForProfile(profile));
      }

      // TODO(sinhak): Remove the if-condition (and use
      // |MirrorAccountReconcilorDelegate|) when all Chrome OS users have been
      // migrated to Account Manager.
      if (chromeos::switches::IsAccountManagerEnabled()) {
        return std::make_unique<ChromeOSAccountReconcilorDelegate>(
            IdentityManagerFactory::GetForProfile(profile),
            chromeos::AccountManagerMigratorFactory::GetForBrowserContext(
                profile));
      }
#elif defined(OS_ANDROID)
      if (base::FeatureList::IsEnabled(signin::kMiceFeature))
        return std::make_unique<signin::MiceAccountReconcilorDelegate>();
#endif
      return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile));

    case signin::AccountConsistencyMethod::kDisabled:
      return std::make_unique<signin::AccountReconcilorDelegate>();

    case signin::AccountConsistencyMethod::kDiceMigration:
    case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      return std::make_unique<signin::DiceAccountReconcilorDelegate>(
          ChromeSigninClientFactory::GetForProfile(profile),
          account_consistency);
#else
      NOTREACHED();
      return nullptr;
#endif
  }

  NOTREACHED();
  return nullptr;
}
