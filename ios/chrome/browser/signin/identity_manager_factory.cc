// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_manager_factory.h"

#include <memory>
#include <utility>

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory_observer.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

namespace {

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    OAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(signin_client, token_service,
                                      account_tracker_service,
                                      image_fetcher::CreateIOSImageDecoder());
  return account_fetcher_service;
}

std::unique_ptr<SigninManager> BuildSigninManager(
    ios::ChromeBrowserState* chrome_browser_state,
    GaiaCookieManagerService* gaia_cookie_manager_service) {
  std::unique_ptr<SigninManager> service = std::make_unique<SigninManager>(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          chrome_browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      gaia_cookie_manager_service, signin::AccountConsistencyMethod::kMirror);
  service->Initialize(GetApplicationContext()->GetLocalState());
  return service;
}
}  // namespace

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to
// ChromeBrowserState without communicating with
// {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  IdentityManagerWrapper(
      std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
      std::unique_ptr<SigninManagerBase> signin_manager,
      std::unique_ptr<AccountFetcherService> account_fetcher_service,
      std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<identity::AccountsCookieMutatorImpl>
          accounts_cookie_mutator,
      std::unique_ptr<identity::DiagnosticsProviderImpl> diagnostics_provider,
      ios::ChromeBrowserState* browser_state)
      : identity::IdentityManager(
            std::move(gaia_cookie_manager_service),
            std::move(signin_manager),
            std::move(account_fetcher_service),
            ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
            ios::AccountTrackerServiceFactory::GetForBrowserState(
                browser_state),
            std::move(primary_account_mutator),
            /*accounts_mutator=*/nullptr,
            std::move(accounts_cookie_mutator),
            std::move(diagnostics_provider)) {}

  // KeyedService overrides.
  void Shutdown() override { IdentityManager::Shutdown(); }
};

void IdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
identity::IdentityManager* IdentityManagerFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  static base::NoDestructor<IdentityManagerFactory> instance;
  return instance.get();
}

// static
void IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  IdentityManagerFactory::GetInstance();
  ios::AccountTrackerServiceFactory::GetInstance();
  ProfileOAuth2TokenServiceFactory::GetInstance();
  SigninClientFactory::GetInstance();
}

void IdentityManagerFactory::AddObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<KeyedService> IdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  // Construct the dependencies that IdentityManager will own.
  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      SigninClientFactory::GetForBrowserState(browser_state));
  std::unique_ptr<SigninManager> signin_manager =
      BuildSigninManager(browser_state, gaia_cookie_manager_service.get());
  auto primary_account_mutator =
      std::make_unique<identity::PrimaryAccountMutatorImpl>(
          ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
          signin_manager.get());
  auto accounts_cookie_mutator =
      std::make_unique<identity::AccountsCookieMutatorImpl>(
          gaia_cookie_manager_service.get());
  auto diagnostics_provider =
      std::make_unique<identity::DiagnosticsProviderImpl>(
          ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
          gaia_cookie_manager_service.get());
  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(
          SigninClientFactory::GetForBrowserState(browser_state),
          ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
          ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state));
  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      std::move(gaia_cookie_manager_service), std::move(signin_manager),
      std::move(account_fetcher_service), std::move(primary_account_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider),
      browser_state);

  for (auto& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager;
}

void IdentityManagerFactory::BrowserStateShutdown(web::BrowserState* context) {
  auto* identity_manager = static_cast<IdentityManagerWrapper*>(
      GetServiceForBrowserState(context, false));
  if (identity_manager) {
    for (auto& observer : observer_list_)
      observer.IdentityManagerShutdown(identity_manager);
  }
  BrowserStateKeyedServiceFactory::BrowserStateShutdown(context);
}
