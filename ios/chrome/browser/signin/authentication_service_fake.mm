// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_fake.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

AuthenticationServiceFake::AuthenticationServiceFake(
    ios::ChromeBrowserState* browser_state)
    : AuthenticationService(
          browser_state,
          OAuth2TokenServiceFactory::GetForBrowserState(browser_state),
          SyncSetupServiceFactory::GetForBrowserState(browser_state)),
      have_accounts_changed_(false) {}

AuthenticationServiceFake::~AuthenticationServiceFake() {}

void AuthenticationServiceFake::SignIn(ChromeIdentity* identity,
                                       const std::string& hosted_domain) {
  authenticated_identity_.reset([identity retain]);
}

void AuthenticationServiceFake::SignOut(
    signin_metrics::ProfileSignout signout_source,
    ProceduralBlock completion) {
  authenticated_identity_.reset();
  if (completion)
    completion();
}

void AuthenticationServiceFake::SetHaveAccountsChanged(bool changed) {
  have_accounts_changed_ = changed;
}

bool AuthenticationServiceFake::HaveAccountsChanged() {
  return have_accounts_changed_;
}

bool AuthenticationServiceFake::IsAuthenticated() {
  return authenticated_identity_.get();
}

ChromeIdentity* AuthenticationServiceFake::GetAuthenticatedIdentity() {
  return authenticated_identity_;
}

NSString* AuthenticationServiceFake::GetAuthenticatedUserEmail() {
  return [authenticated_identity_ userEmail];
}

std::unique_ptr<KeyedService>
AuthenticationServiceFake::CreateAuthenticationService(
    web::BrowserState* browser_state) {
  return base::WrapUnique(new AuthenticationServiceFake(
      ios::ChromeBrowserState::FromBrowserState(browser_state)));
}
