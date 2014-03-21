// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/profile_invalidation_auth_provider.h"

#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/signin/core/profile_oauth2_token_service.h"

namespace invalidation {

ProfileInvalidationAuthProvider::ProfileInvalidationAuthProvider(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    LoginUIService* login_ui_service)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      login_ui_service_(login_ui_service) {
  signin_manager_->AddObserver(this);
}

ProfileInvalidationAuthProvider::~ProfileInvalidationAuthProvider() {
  signin_manager_->RemoveObserver(this);
}

std::string ProfileInvalidationAuthProvider::GetAccountId() {
  return signin_manager_->GetAuthenticatedAccountId();
}

OAuth2TokenService* ProfileInvalidationAuthProvider::GetTokenService() {
  return token_service_;
}

bool ProfileInvalidationAuthProvider::ShowLoginUI() {
  login_ui_service_->ShowLoginPopup();
  return true;
}

void ProfileInvalidationAuthProvider::GoogleSignedOut(
    const std::string& username) {
  FireInvalidationAuthLogout();
}

}  // namespace invalidation
