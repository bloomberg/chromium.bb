// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerService(
    web::BrowserState* browser_state) {
  return BuildFakeGaiaCookieManagerServiceWithOptions(
      /*create_fake_url_loader_factory_for_cookie_requests=*/true,
      browser_state);
}

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceWithOptions(
    bool create_fake_url_loader_factory_for_cookie_requests,
    web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  return std::make_unique<FakeGaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          chrome_browser_state),
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      create_fake_url_loader_factory_for_cookie_requests);
}
