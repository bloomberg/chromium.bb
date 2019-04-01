// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

#if !defined(OS_CHROMEOS)
#include "services/identity/public/cpp/primary_account_mutator_impl.h"
#endif

#if !defined(OS_ANDROID)
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#endif

namespace {

#if !defined(OS_CHROMEOS)
using ConcreteSigninManager = SigninManager;
#else
using ConcreteSigninManager = SigninManagerBase;
#endif

// Helper function returning a newly constructed PrimaryAccountMutator for
// |profile|.  May return null if mutation of the signed-in state is not
// supported on the current platform.
std::unique_ptr<identity::PrimaryAccountMutator> BuildPrimaryAccountMutator(
    Profile* profile,
    ConcreteSigninManager* signin_manager) {
#if !defined(OS_CHROMEOS)
  return std::make_unique<identity::PrimaryAccountMutatorImpl>(
      AccountTrackerServiceFactory::GetForProfile(profile),
      SigninManager::FromSigninManagerBase(signin_manager));
#else
  return nullptr;
#endif
}

// Helper function returning a newly constructed AccountsMutator for
// |profile|. May return null if mutation of accounts is not supported on the
// current platform.
std::unique_ptr<identity::AccountsMutator> BuildAccountsMutator(
    Profile* profile,
    SigninManagerBase* signin_manager) {
#if !defined(OS_ANDROID)
  return std::make_unique<identity::AccountsMutatorImpl>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile), signin_manager,
      profile->GetPrefs());
#else
  return nullptr;
#endif
}

std::unique_ptr<ConcreteSigninManager> BuildSigninManager(
    Profile* profile,
    GaiaCookieManagerService* gaia_cookie_manager_service) {
  std::unique_ptr<ConcreteSigninManager> signin_manager;
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
#if defined(OS_CHROMEOS)
  signin_manager = std::make_unique<ConcreteSigninManager>(
      client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile));
#else
  signin_manager = std::make_unique<ConcreteSigninManager>(
      client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile),
      gaia_cookie_manager_service,
      AccountConsistencyModeManager::GetMethodForProfile(profile));
#endif
  signin_manager->Initialize(g_browser_process->local_state());
  return signin_manager;
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    OAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(
      signin_client, token_service, account_tracker_service,
      std::make_unique<suggestions::ImageDecoderImpl>());
  return account_fetcher_service;
}

}  // namespace

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to Profile
// without communicating with {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  explicit IdentityManagerWrapper(
      Profile* profile,
      std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
      std::unique_ptr<SigninManagerBase> signin_manager,
      std::unique_ptr<AccountFetcherService> account_fetcher_service,
      std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<identity::AccountsMutator> accounts_mutator,
      std::unique_ptr<identity::AccountsCookieMutatorImpl>
          accounts_cookie_mutator,
      std::unique_ptr<identity::DiagnosticsProviderImpl> diagnostics_provider)
      : identity::IdentityManager(
            std::move(gaia_cookie_manager_service),
            std::move(signin_manager),
            std::move(account_fetcher_service),
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            AccountTrackerServiceFactory::GetForProfile(profile),
            std::move(primary_account_mutator),
            std::move(accounts_mutator),
            std::move(accounts_cookie_mutator),
            std::move(diagnostics_provider)) {}

  // KeyedService overrides.
  void Shutdown() override { IdentityManager::Shutdown(); }
};

void IdentityManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IdentityManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfileIfExists(
    const Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(const_cast<Profile*>(profile),
                                                 false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  return base::Singleton<IdentityManagerFactory>::get();
}

// static
void IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  IdentityManagerFactory::GetInstance();
  AccountTrackerServiceFactory::GetInstance();
  ChromeSigninClientFactory::GetInstance();
  ProfileOAuth2TokenServiceFactory::GetInstance();
}

void IdentityManagerFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

KeyedService* IdentityManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Construct the dependencies that IdentityManager will own.
  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile));
  std::unique_ptr<ConcreteSigninManager> signin_manager =
      BuildSigninManager(profile, gaia_cookie_manager_service.get());
  std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator =
      BuildPrimaryAccountMutator(profile, signin_manager.get());
  std::unique_ptr<identity::AccountsMutator> accounts_mutator =
      BuildAccountsMutator(profile, signin_manager.get());
  auto accounts_cookie_mutator =
      std::make_unique<identity::AccountsCookieMutatorImpl>(
          gaia_cookie_manager_service.get());
  auto diagnostics_provider =
      std::make_unique<identity::DiagnosticsProviderImpl>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          gaia_cookie_manager_service.get());
  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(
          ChromeSigninClientFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          AccountTrackerServiceFactory::GetForProfile(profile));

  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      profile, std::move(gaia_cookie_manager_service),
      std::move(signin_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator), std::move(accounts_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider));
  for (Observer& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());
  return identity_manager.release();
}

void IdentityManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* identity_manager = static_cast<IdentityManagerWrapper*>(
      GetServiceForBrowserContext(context, false));
  if (identity_manager) {
    for (Observer& observer : observer_list_)
      observer.IdentityManagerShutdown(identity_manager);
  }
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}
