// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/signin_manager.h"

#include "base/string_util.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"

// static
void SigninManager::RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterStringPref(prefs::kGoogleServicesUsername, "");
}

void SigninManager::Initialize(Profile* profile) {
  profile_ = profile;
  username_ = profile_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  profile_->GetTokenService()->Initialize(
      GaiaConstants::kChromeSource, profile_);
  if (!username_.empty()) {
    profile_->GetTokenService()->LoadTokensFromDB();
  }
}

// If a username already exists, the user is logged in.
const std::string& SigninManager::GetUsername() {
  return username_;
}

void SigninManager::SetUsername(const std::string& username) {
  username_ = username;
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(username_.empty());
  // The Sign out should clear the token service credentials.
  DCHECK(!profile_->GetTokenService()->AreCredentialsValid());

  username_.assign(username);
  password_.assign(password);

  client_login_.reset(new GaiaAuthenticator2(this,
                                             GaiaConstants::kChromeSource,
                                             profile_->GetRequestContext()));
  client_login_->StartClientLogin(username,
                                  password,
                                  "",
                                  login_token,
                                  login_captcha);
}

void SigninManager::SignOut() {
  client_login_.reset();
  username_.clear();
  password_.clear();
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, username_);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
  profile_->GetTokenService()->ResetCredentialsInMemory();
  profile_->GetTokenService()->EraseTokensFromDB();
}

void SigninManager::OnClientLoginSuccess(const ClientLoginResult& result) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, username_);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  GoogleServiceSigninSuccessDetails details(username_, password_);
  NotificationService::current()->Notify(
      NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
      Source<SigninManager>(this),
      Details<const GoogleServiceSigninSuccessDetails>(&details));

  password_.clear();  // Don't need it anymore.

  profile_->GetTokenService()->UpdateCredentials(result);
  DCHECK(profile_->GetTokenService()->AreCredentialsValid());
  profile_->GetTokenService()->StartFetchingTokens();
}

void SigninManager::OnClientLoginFailure(const GoogleServiceAuthError& error) {
  GoogleServiceAuthError details(error);
  NotificationService::current()->Notify(
      NotificationType::GOOGLE_SIGNIN_FAILED,
      Source<SigninManager>(this),
      Details<const GoogleServiceAuthError>(&details));
  SignOut();
}
