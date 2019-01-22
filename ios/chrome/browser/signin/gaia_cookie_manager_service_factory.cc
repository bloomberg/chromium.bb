// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_cookie_manager_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

namespace ios {

GaiaCookieManagerServiceFactory::GaiaCookieManagerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GaiaCookieManagerService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

GaiaCookieManagerServiceFactory::~GaiaCookieManagerServiceFactory() {}

// static
GaiaCookieManagerService* GaiaCookieManagerServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<GaiaCookieManagerService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
GaiaCookieManagerServiceFactory*
GaiaCookieManagerServiceFactory::GetInstance() {
  static base::NoDestructor<GaiaCookieManagerServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
GaiaCookieManagerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<GaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          chrome_browser_state),
      SigninClientFactory::GetForBrowserState(chrome_browser_state));
}

}  // namespace ios
