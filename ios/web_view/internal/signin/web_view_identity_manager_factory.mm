// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/signin/web_view_account_fetcher_service_factory.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_gaia_cookie_manager_service_factory.h"
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
std::unique_ptr<SigninManager> BuildSigninManager(
    WebViewBrowserState* browser_state) {
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
      WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(browser_state),
      signin::AccountConsistencyMethod::kDisabled);
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
      std::unique_ptr<SigninManagerBase> signin_manager,
      std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
      WebViewBrowserState* browser_state)
      : identity::IdentityManager(
            std::move(signin_manager),
            WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
            WebViewAccountFetcherServiceFactory::GetForBrowserState(
                browser_state),
            WebViewAccountTrackerServiceFactory::GetForBrowserState(
                browser_state),
            WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(
                browser_state),
            std::move(primary_account_mutator),
            /*accounts_mutator=*/nullptr,
            std::make_unique<identity::AccountsCookieMutatorImpl>(
                WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(
                    browser_state)),
            std::make_unique<identity::DiagnosticsProviderImpl>(
                WebViewOAuth2TokenServiceFactory::GetForBrowserState(
                    browser_state),
                WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(
                    browser_state))) {}
};

void WebViewIdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninManagerBase::RegisterProfilePrefs(registry);
}

WebViewIdentityManagerFactory::WebViewIdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAccountFetcherServiceFactory::GetInstance());
  DependsOn(WebViewAccountTrackerServiceFactory::GetInstance());
  DependsOn(WebViewGaiaCookieManagerServiceFactory::GetInstance());
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
  WebViewGaiaCookieManagerServiceFactory::GetInstance();
  WebViewOAuth2TokenServiceFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
}

std::unique_ptr<KeyedService>
WebViewIdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  // Construct the dependencies that IdentityManager will own.
  std::unique_ptr<SigninManager> signin_manager =
      BuildSigninManager(browser_state);
  auto primary_account_mutator =
      std::make_unique<identity::PrimaryAccountMutatorImpl>(
          WebViewAccountTrackerServiceFactory::GetForBrowserState(
              browser_state),
          signin_manager.get());
  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      std::move(signin_manager), std::move(primary_account_mutator),
      browser_state);
  return identity_manager;
}

}  // namespace ios_web_view
