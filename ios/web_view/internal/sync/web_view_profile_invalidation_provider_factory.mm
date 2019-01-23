// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/impl/ticl_profile_settings_provider.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_manager_factory.h"
#include "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using invalidation::InvalidatorStorage;
using invalidation::ProfileInvalidationProvider;
using invalidation::TiclInvalidationService;

namespace ios_web_view {

namespace {

void RequestProxyResolvingSocketFactoryOnUIThread(
    WebViewBrowserState* browser_state,
    base::WeakPtr<TiclInvalidationService> service,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  if (!service)
    return;
  browser_state->GetProxyResolvingSocketFactory(std::move(request));
}

// A thread-safe wrapper to request a ProxyResolvingSocketFactoryPtr.
void RequestProxyResolvingSocketFactory(
    WebViewBrowserState* browser_state,
    base::WeakPtr<TiclInvalidationService> service,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI},
      base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                     browser_state, std::move(service), std::move(request)));
}
}

// static
invalidation::ProfileInvalidationProvider*
WebViewProfileInvalidationProviderFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewProfileInvalidationProviderFactory*
WebViewProfileInvalidationProviderFactory::GetInstance() {
  static base::NoDestructor<WebViewProfileInvalidationProviderFactory> instance;
  return instance.get();
}

WebViewProfileInvalidationProviderFactory::
    WebViewProfileInvalidationProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "InvalidationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewSigninManagerFactory::GetInstance());
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
}

WebViewProfileInvalidationProviderFactory::
    ~WebViewProfileInvalidationProviderFactory() {}

std::unique_ptr<KeyedService>
WebViewProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  std::unique_ptr<invalidation::ProfileIdentityProvider> identity_provider(
      new invalidation::ProfileIdentityProvider(
          WebViewIdentityManagerFactory::GetForBrowserState(browser_state)));

  std::unique_ptr<TiclInvalidationService> service(new TiclInvalidationService(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE),
      identity_provider.get(),
      std::make_unique<invalidation::TiclProfileSettingsProvider>(
          browser_state->GetPrefs()),
      WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, browser_state),
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::IO}),
      browser_state->GetSharedURLLoaderFactory(),
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker()));
  service->Init(
      std::make_unique<InvalidatorStorage>(browser_state->GetPrefs()));

  return std::make_unique<ProfileInvalidationProvider>(
      std::move(service), std::move(identity_provider));
}

void WebViewProfileInvalidationProviderFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
  InvalidatorStorage::RegisterProfilePrefs(registry);
}

}  // namespace ios_web_view
