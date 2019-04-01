// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"

#include <memory>

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

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
    WebViewBrowserState* browser_state,
    GaiaCookieManagerService* gaia_cookie_manager_service) {
  // Clearing the sign in state on start up greatly simplifies the management of
  // ChromeWebView's signin state.
  PrefService* pref_service = browser_state->GetPrefs();
  pref_service->ClearPref(prefs::kGoogleServicesAccountId);
  pref_service->ClearPref(prefs::kGoogleServicesUsername);
  pref_service->ClearPref(prefs::kGoogleServicesUserAccountId);

  std::unique_ptr<SigninManager> service = std::make_unique<SigninManager>(
      WebViewSigninClientFactory::GetForBrowserState(browser_state),
      WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      WebViewAccountTrackerServiceFactory::GetForBrowserState(browser_state),
      gaia_cookie_manager_service, signin::AccountConsistencyMethod::kDisabled);
  service->Initialize(ApplicationContext::GetInstance()->GetLocalState());
  return service;
}
}  // namespace

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to
// WebViewBrowserState without communicating with
// {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  explicit IdentityManagerWrapper(
      std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
      std::unique_ptr<SigninManagerBase> signin_manager,
      std::unique_ptr<AccountFetcherService> account_fetcher_service,
      std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<identity::AccountsCookieMutatorImpl>
          accounts_cookie_mutator,
      std::unique_ptr<identity::DiagnosticsProviderImpl> diagnostics_provider,
      WebViewBrowserState* browser_state)
      : identity::IdentityManager(
            std::move(gaia_cookie_manager_service),
            std::move(signin_manager),
            std::move(account_fetcher_service),
            WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
            WebViewAccountTrackerServiceFactory::GetForBrowserState(
                browser_state),
            std::move(primary_account_mutator),
            /*accounts_mutator=*/nullptr,
            std::move(accounts_cookie_mutator),
            std::move(diagnostics_provider)) {}

  // KeyedService overrides.
  void Shutdown() override { IdentityManager::Shutdown(); }
};

void WebViewIdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
}

WebViewIdentityManagerFactory::WebViewIdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAccountTrackerServiceFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
  DependsOn(WebViewSigninClientFactory::GetInstance());
}

WebViewIdentityManagerFactory::~WebViewIdentityManagerFactory() {}

// static
identity::IdentityManager* WebViewIdentityManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewIdentityManagerFactory* WebViewIdentityManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewIdentityManagerFactory> instance;
  return instance.get();
}

// static
void WebViewIdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  WebViewIdentityManagerFactory::GetInstance();
  WebViewAccountTrackerServiceFactory::GetInstance();
  WebViewOAuth2TokenServiceFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
}

std::unique_ptr<KeyedService>
WebViewIdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  // Construct the dependencies that IdentityManager will own.
  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      WebViewSigninClientFactory::GetForBrowserState(browser_state));
  std::unique_ptr<SigninManager> signin_manager =
      BuildSigninManager(browser_state, gaia_cookie_manager_service.get());
  auto primary_account_mutator =
      std::make_unique<identity::PrimaryAccountMutatorImpl>(
          WebViewAccountTrackerServiceFactory::GetForBrowserState(
              browser_state),
          signin_manager.get());
  auto accounts_cookie_mutator =
      std::make_unique<identity::AccountsCookieMutatorImpl>(
          gaia_cookie_manager_service.get());
  auto diagnostics_provider =
      std::make_unique<identity::DiagnosticsProviderImpl>(
          WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
          gaia_cookie_manager_service.get());
  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(
          WebViewSigninClientFactory::GetForBrowserState(browser_state),
          WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
          WebViewAccountTrackerServiceFactory::GetForBrowserState(
              browser_state));
  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      std::move(gaia_cookie_manager_service), std::move(signin_manager),
      std::move(account_fetcher_service), std::move(primary_account_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider),
      browser_state);
  return identity_manager;
}

}  // namespace ios_web_view
