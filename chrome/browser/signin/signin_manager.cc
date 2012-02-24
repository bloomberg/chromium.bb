// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

const char kGetInfoEmailKey[] = "email";
const char kGetInfoServicesKey[] = "allServices";
const char kGooglePlusServiceKey[] = "googleme";

SigninManager::SigninManager()
    : profile_(NULL),
      had_two_factor_error_(false),
      last_login_auth_error_(GoogleServiceAuthError::None()) {
}

SigninManager::~SigninManager() {}

void SigninManager::Initialize(Profile* profile) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  profile_ = profile;

  std::string user = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  if (!user.empty())
    SetAuthenticatedUsername(user);
  // TokenService can be null for unit tests.
  TokenService* token_service = profile_->GetTokenService();
  if (token_service) {
    token_service->Initialize(GaiaConstants::kChromeSource, profile_);
    if (!authenticated_username_.empty()) {
      token_service->LoadTokensFromDB();
    }
  }
}

bool SigninManager::IsInitialized() const {
  return profile_ != NULL;
}

void SigninManager::CleanupNotificationRegistration() {
#if !defined(OS_CHROMEOS)
  content::Source<TokenService> token_service(profile_->GetTokenService());
  if (registrar_.IsRegistered(this,
                              chrome::NOTIFICATION_TOKEN_AVAILABLE,
                              token_service)) {
    registrar_.Remove(this,
                      chrome::NOTIFICATION_TOKEN_AVAILABLE,
                      token_service);
  }
#endif
}

const std::string& SigninManager::GetAuthenticatedUsername() {
  return authenticated_username_;
}

void SigninManager::SetAuthenticatedUsername(const std::string& username) {
  if (!authenticated_username_.empty()) {
    DLOG_IF(ERROR, username != authenticated_username_) <<
        "Tried to change the authenticated username to something different: " <<
        "Current: " << authenticated_username_ << ", New: " << username;
    return;
  }
  authenticated_username_ = username;
  // TODO(tim): We could go further in ensuring kGoogleServicesUsername and
  // authenticated_username_ are consistent once established (e.g. remove
  // authenticated_username_ altogether). Bug 107160.
}

void SigninManager::PrepareForSignin() {
  DCHECK(possibly_invalid_username_.empty());
  // This attempt is either 1) the user trying to establish initial sync, or
  // 2) trying to refresh credentials for an existing username.  If it is 2, we
  // need to try again, but take care to leave state around tracking that the
  // user has successfully signed in once before with this username, so that on
  // restart we don't think sync setup has never completed.
  ClearTransientSigninData();
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(authenticated_username_.empty() ||
         username == authenticated_username_);
  PrepareForSignin();
  possibly_invalid_username_.assign(username);
  password_.assign(password);

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  client_login_->StartClientLogin(username,
                                  password,
                                  "",
                                  login_token,
                                  login_captcha,
                                  GaiaAuthFetcher::HostedAccountsNotAllowed);

  // Register for token availability.  The signin manager will pre-login the
  // user when the GAIA service token is ready for use.  Only do this if we
  // are not running in ChomiumOS, since it handles pre-login itself.
#if !defined(OS_CHROMEOS)
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(profile_->GetTokenService()));
#endif
}

void SigninManager::ProvideSecondFactorAccessCode(
    const std::string& access_code) {
  DCHECK(!possibly_invalid_username_.empty() && !password_.empty() &&
      last_result_.data.empty());

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  client_login_->StartClientLogin(possibly_invalid_username_,
                                  access_code,
                                  "",
                                  std::string(),
                                  std::string(),
                                  GaiaAuthFetcher::HostedAccountsNotAllowed);
}

void SigninManager::ClearTransientSigninData() {
  DCHECK(IsInitialized());

  CleanupNotificationRegistration();
  client_login_.reset();
  last_result_ = ClientLoginResult();
  possibly_invalid_username_.clear();
  password_.clear();
  had_two_factor_error_ = false;
}

void SigninManager::SignOut() {
  DCHECK(IsInitialized());
  if (authenticated_username_.empty() && !client_login_.get()) {
    // Just exit if we aren't signed in (or in the process of signing in).
    // This avoids a perf regression because SignOut() is invoked on startup to
    // clean up any incomplete previous signin attempts.
    return;
  }

  ClearTransientSigninData();
  authenticated_username_.clear();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  profile_->GetPrefs()->ClearPref(prefs::kIsGooglePlusUser);
  profile_->GetTokenService()->ResetCredentialsInMemory();
  profile_->GetTokenService()->EraseTokensFromDB();
}

const GoogleServiceAuthError& SigninManager::GetLoginAuthError() const {
  return last_login_auth_error_;
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_username_.empty();
}

void SigninManager::OnClientLoginSuccess(const ClientLoginResult& result) {
  last_result_ = result;
  // Make a request for the canonical email address and services.
  client_login_->StartGetUserInfo(result.lsid);
}

void SigninManager::OnGetUserInfoSuccess(const UserInfoMap& data) {
  UserInfoMap::const_iterator email_iter = data.find(kGetInfoEmailKey);
  if (email_iter == data.end()) {
    OnGetUserInfoKeyNotFound(kGetInfoEmailKey);
    return;
  } else {
    DCHECK(email_iter->first == kGetInfoEmailKey);
    last_login_auth_error_ = GoogleServiceAuthError::None();
    SetAuthenticatedUsername(email_iter->second);
    possibly_invalid_username_.clear();
    profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                    authenticated_username_);

  }
  UserInfoMap::const_iterator service_iter = data.find(kGetInfoServicesKey);
  if (service_iter == data.end()) {
    DLOG(WARNING) << "Could not retrieve services for account with email: "
             << authenticated_username_ <<".";
  } else {
    DCHECK(service_iter->first == kGetInfoServicesKey);
    std::vector<std::string> services;
    base::SplitStringUsingSubstr(service_iter->second, ", ", &services);
    std::vector<std::string>::const_iterator iter =
        std::find(services.begin(), services.end(), kGooglePlusServiceKey);
    bool isGPlusUser = (iter != services.end());
    profile_->GetPrefs()->SetBoolean(prefs::kIsGooglePlusUser, isGPlusUser);
  }
  GoogleServiceSigninSuccessDetails details(authenticated_username_,
                                            password_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  password_.clear();  // Don't need it anymore.

  profile_->GetTokenService()->UpdateCredentials(last_result_);
  DCHECK(profile_->GetTokenService()->AreCredentialsValid());
  profile_->GetTokenService()->StartFetchingTokens();
}

void SigninManager::OnGetUserInfoKeyNotFound(const std::string& key) {
  DCHECK(key == kGetInfoEmailKey);
  LOG(ERROR) << "Account is not associated with a valid email address. "
             << "Login failed.";
  OnClientLoginFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void SigninManager::OnGetUserInfoFailure(const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Unable to retreive the canonical email address. Login failed.";
  OnClientLoginFailure(error);
}

void SigninManager::OnTokenAuthFailure(const GoogleServiceAuthError& error) {
#if !defined(OS_CHROMEOS)
  DVLOG(1) << "Unable to retrieve the token auth.";
  CleanupNotificationRegistration();
#endif
}

void SigninManager::OnClientLoginFailure(const GoogleServiceAuthError& error) {
  // If we got a bad ASP, prompt for an ASP again by forcing another TWO_FACTOR
  // error.
  bool invalid_gaia = error.state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS;
  last_login_auth_error_ = (invalid_gaia && had_two_factor_error_) ?
      GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR) : error;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceAuthError>(&last_login_auth_error_));

  // We don't sign-out if the password was valid and we're just dealing with
  // a second factor error, and we don't sign out if we're dealing with
  // an invalid access code (again, because the password was valid).
  if (last_login_auth_error_.state() == GoogleServiceAuthError::TWO_FACTOR) {
    had_two_factor_error_ = true;
    return;
  }

  ClearTransientSigninData();
}

void SigninManager::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
#if !defined(OS_CHROMEOS)
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE);
  TokenService::TokenAvailableDetails* tok_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();

  // If a GAIA service token has become available, use it to pre-login the
  // user to other services that depend on GAIA credentials.
  if (tok_details->service() == GaiaConstants::kGaiaService) {
    if (client_login_.get() == NULL) {
      client_login_.reset(new GaiaAuthFetcher(this,
                                              GaiaConstants::kChromeSource,
                                              profile_->GetRequestContext()));
    }

    client_login_->StartMergeSession(tok_details->token());

    // We only want to do this once per sign-in.
    CleanupNotificationRegistration();
  }
#endif
}
