// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_identity_provider.h"

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#endif

ProfileIdentityProvider::ProfileIdentityProvider(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    LoginUIService* login_ui_service)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      login_ui_service_(login_ui_service) {
  signin_manager_->AddObserver(this);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  signin_manager_->RemoveObserver(this);
}

std::string ProfileIdentityProvider::GetActiveUsername() {
  return signin_manager_->GetAuthenticatedUsername();
}

std::string ProfileIdentityProvider::GetActiveAccountId() {
  return signin_manager_->GetAuthenticatedAccountId();
}

OAuth2TokenService* ProfileIdentityProvider::GetTokenService() {
  return token_service_;
}

bool ProfileIdentityProvider::RequestLogin() {
#if defined(OS_ANDROID)
  return false;
#else
  login_ui_service_->ShowLoginPopup();
  return true;
#endif
}

void ProfileIdentityProvider::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  FireOnActiveAccountLogin();
}

void ProfileIdentityProvider::GoogleSignedOut(const std::string& account_id,
                                              const std::string& username) {
  FireOnActiveAccountLogout();
}
