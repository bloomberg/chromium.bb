// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_account_reconcilor.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"

FakeAccountReconcilor::FakeAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client) :
  AccountReconcilor(token_service, signin_manager, client) {}


// static
KeyedService* FakeAccountReconcilor::Build(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountReconcilor* reconcilor = new FakeAccountReconcilor(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

void FakeAccountReconcilor::GetAccountsFromCookie(
    GetAccountsFromCookieCallback callback) {
  std::vector<std::pair<std::string, bool> > gaia_accounts;
  callback.Run(GoogleServiceAuthError::AuthErrorNone(), gaia_accounts);
}
