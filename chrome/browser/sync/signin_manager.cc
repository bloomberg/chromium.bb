// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/signin_manager.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

const char kGetInfoEmailKey[] = "email";

SigninManager::SigninManager()
    : profile_(NULL), had_two_factor_error_(false) {}

SigninManager::~SigninManager() {}

// static
void SigninManager::RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kSyncUsingOAuth,
                                  "",
                                  PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterStringPref(prefs::kGoogleServicesUsername,
                                 "",
                                 PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kAutologinEnabled,
                                  true,
                                  PrefService::UNSYNCABLE_PREF);
}

void SigninManager::Initialize(Profile* profile) {
  profile_ = profile;
  SetUsername(profile_->GetPrefs()->GetString(prefs::kGoogleServicesUsername));
  profile_->GetTokenService()->Initialize(
      GaiaConstants::kChromeSource, profile_);
  if (!GetUsername().empty()) {
    profile_->GetTokenService()->LoadTokensFromDB();
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

// If a username already exists, the user is logged in.
const std::string& SigninManager::GetUsername() {
  return browser_sync::IsUsingOAuth() ? oauth_username_ : username_;
}

void SigninManager::SetUsername(const std::string& username) {
  if (browser_sync::IsUsingOAuth())
    oauth_username_ = username;
  else
    username_ = username;
}

// static
void SigninManager::PrepareForSignin() {
  DCHECK(!browser_sync::IsUsingOAuth());
  DCHECK(username_.empty());
}

// static
void SigninManager::PrepareForOAuthSignin() {
  DCHECK(browser_sync::IsUsingOAuth());
  DCHECK(oauth_username_.empty());
#if !defined(OS_CHROMEOS)
  // The Sign out should clear the token service credentials.
  // Note: In CHROMEOS we might have valid credentials but still need to
  // set up 2-factor authentication.
  DCHECK(!profile_->GetTokenService()->HasOAuthCredentials());
#endif
}

// Users must always sign out before they sign in again.
void SigninManager::StartOAuthSignIn(const std::string& oauth1_request_token) {
  DCHECK(browser_sync::IsUsingOAuth());
  PrepareForOAuthSignin();
  oauth1_request_token_.assign(oauth1_request_token);
  oauth_login_.reset(new GaiaOAuthFetcher(this,
                                          profile_->GetRequestContext(),
                                          profile_,
                                          GaiaConstants::kSyncServiceOAuth));
  oauth_login_->StartOAuthGetAccessToken(oauth1_request_token_);
  // TODO(rogerta?): Bug 92325: Expand Autologin to include OAuth signin
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(!browser_sync::IsUsingOAuth());
  PrepareForSignin();
  username_.assign(username);
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
  DCHECK(!browser_sync::IsUsingOAuth());
  DCHECK(!username_.empty() && !password_.empty() &&
      last_result_.data.empty());

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  client_login_->StartClientLogin(username_,
                                  access_code,
                                  "",
                                  std::string(),
                                  std::string(),
                                  GaiaAuthFetcher::HostedAccountsNotAllowed);
}

void SigninManager::ClearInMemoryData() {
  if (!profile_)
    return;

  CleanupNotificationRegistration();
  client_login_.reset();
  last_result_ = ClientLoginResult();
  username_.clear();
  oauth_username_.clear();
  password_.clear();
  had_two_factor_error_ = false;
}

void SigninManager::SignOut() {
  if (!profile_)
    return;

  ClearInMemoryData();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  profile_->GetPrefs()->ClearPref(prefs::kSyncUsingOAuth);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
  profile_->GetTokenService()->ResetCredentialsInMemory();
  profile_->GetTokenService()->EraseTokensFromDB();
}

void SigninManager::OnClientLoginSuccess(const ClientLoginResult& result) {
  DCHECK(!browser_sync::IsUsingOAuth());
  last_result_ = result;
  // Make a request for the canonical email address.
  client_login_->StartGetUserInfo(result.lsid, kGetInfoEmailKey);
}

// NOTE: GetUserInfo is a ClientLogin request similar to OAuth's userinfo
void SigninManager::OnGetUserInfoSuccess(const std::string& key,
                                         const std::string& value) {
  DCHECK(!browser_sync::IsUsingOAuth());
  DCHECK(key == kGetInfoEmailKey);

  username_ = value;
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, username_);
  profile_->GetPrefs()->SetBoolean(prefs::kSyncUsingOAuth, false);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  GoogleServiceSigninSuccessDetails details(username_, password_);
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
  DCHECK(!browser_sync::IsUsingOAuth());
  DCHECK(key == kGetInfoEmailKey);
  LOG(ERROR) << "Account is not associated with a valid email address. "
             << "Login failed.";
  OnClientLoginFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void SigninManager::OnGetUserInfoFailure(const GoogleServiceAuthError& error) {
  DCHECK(!browser_sync::IsUsingOAuth());
  LOG(ERROR) << "Unable to retreive the canonical email address. Login failed.";
  OnClientLoginFailure(error);
}

void SigninManager::OnTokenAuthFailure(const GoogleServiceAuthError& error) {
  DCHECK(!browser_sync::IsUsingOAuth());
#if !defined(OS_CHROMEOS)
  DVLOG(1) << "Unable to retrieve the token auth.";
  CleanupNotificationRegistration();
#endif
}

void SigninManager::OnClientLoginFailure(const GoogleServiceAuthError& error) {
  DCHECK(!browser_sync::IsUsingOAuth());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceAuthError>(&error));

  // We don't sign-out if the password was valid and we're just dealing with
  // a second factor error, and we don't sign out if we're dealing with
  // an invalid access code (again, because the password was valid).
  bool invalid_gaia = error.state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS;
  if (error.state() == GoogleServiceAuthError::TWO_FACTOR ||
      (had_two_factor_error_ && invalid_gaia)) {
    had_two_factor_error_ = true;
    return;
  }

  ClearInMemoryData();
}

void SigninManager::OnOAuthGetAccessTokenSuccess(const std::string& token,
                                                 const std::string& secret) {
  DCHECK(browser_sync::IsUsingOAuth());
  DVLOG(1) << "SigninManager::OnOAuthGetAccessTokenSuccess";
  profile_->GetTokenService()->UpdateOAuthCredentials(token, secret);
}

void SigninManager::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(browser_sync::IsUsingOAuth());
  LOG(WARNING) << "SigninManager::OnOAuthGetAccessTokenFailure";
}

void SigninManager::OnOAuthWrapBridgeSuccess(const std::string& service_name,
                                             const std::string& token,
                                             const std::string& expires_in) {
  DCHECK(browser_sync::IsUsingOAuth());
  DVLOG(1) << "SigninManager::OnOAuthWrapBridgeSuccess";
}

void SigninManager::OnOAuthWrapBridgeFailure(
    const std::string& service_scope,
    const GoogleServiceAuthError& error) {
  DCHECK(browser_sync::IsUsingOAuth());
  LOG(WARNING) << "SigninManager::OnOAuthWrapBridgeFailure";
}

// NOTE: userinfo is an OAuth request similar to ClientLogin's GetUserInfo
void SigninManager::OnUserInfoSuccess(const std::string& email) {
  DCHECK(browser_sync::IsUsingOAuth());

  TokenService* token_service = profile_->GetTokenService();
  CHECK(token_service);

  // If |SignOut()| was called between the login start and |OnUserInfoSucess()|,
  // then the OAuth credentials would have been cleared.
  if (!token_service->HasOAuthCredentials())
    return;

  DVLOG(1) << "Sync signin for " << email << " is complete.";
  oauth_username_ = email;
  profile_->GetPrefs()->SetString(
      prefs::kGoogleServicesUsername, oauth_username_);
  profile_->GetPrefs()->SetBoolean(prefs::kSyncUsingOAuth, true);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  DCHECK(password_.empty());
  GoogleServiceSigninSuccessDetails details(oauth_username_, "");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  DCHECK(token_service->HasOAuthCredentials());
  token_service->StartFetchingOAuthTokens();
}

void SigninManager::OnUserInfoFailure(const GoogleServiceAuthError& error) {
  DCHECK(browser_sync::IsUsingOAuth());
  LOG(WARNING) << "SigninManager::OnUserInfoFailure";
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
    DCHECK(!browser_sync::IsUsingOAuth());
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
