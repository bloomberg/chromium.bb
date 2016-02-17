// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/fake_oauth2_token_service_builder.h"

#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/chrome/browser/signin/signin_error_controller_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

scoped_ptr<KeyedService> BuildFakeOAuth2TokenService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  OAuth2TokenServiceDelegate* delegate =
      new FakeProfileOAuth2TokenServiceIOSDelegate(
          SigninClientFactory::GetForBrowserState(browser_state),
          ios::GetChromeBrowserProvider()
              ->GetProfileOAuth2TokenServiceIOSProvider(),
          ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
          ios::SigninErrorControllerFactory::GetForBrowserState(browser_state));
  return make_scoped_ptr(new FakeProfileOAuth2TokenService(delegate));
}
