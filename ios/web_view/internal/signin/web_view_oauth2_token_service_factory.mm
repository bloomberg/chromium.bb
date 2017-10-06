// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_web_view_configuration+sync.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewOAuth2TokenServiceFactory::WebViewOAuth2TokenServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProfileOAuth2TokenService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAccountTrackerServiceFactory::GetInstance());
  DependsOn(WebViewSigninClientFactory::GetInstance());
  DependsOn(WebViewSigninErrorControllerFactory::GetInstance());
}

ProfileOAuth2TokenService* WebViewOAuth2TokenServiceFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<ProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewOAuth2TokenServiceFactory*
WebViewOAuth2TokenServiceFactory::GetInstance() {
  return base::Singleton<WebViewOAuth2TokenServiceFactory>::get();
}

void WebViewOAuth2TokenServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
WebViewOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  CWVWebViewConfiguration* web_view_configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  DCHECK_EQ(web_view_configuration.browserState, browser_state);
  auto delegate = base::MakeUnique<ProfileOAuth2TokenServiceIOSDelegate>(
      WebViewSigninClientFactory::GetForBrowserState(browser_state),
      base::MakeUnique<WebViewProfileOAuth2TokenServiceIOSProviderImpl>(
          web_view_configuration.authenticationController),
      WebViewAccountTrackerServiceFactory::GetForBrowserState(browser_state),
      WebViewSigninErrorControllerFactory::GetForBrowserState(browser_state));
  return base::MakeUnique<ProfileOAuth2TokenService>(std::move(delegate));
}

}  // namespace ios_web_view
