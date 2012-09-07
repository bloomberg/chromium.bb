// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "unicode/regex.h"

namespace {

const char kGetInfoEmailKey[] = "email";
const char kGetInfoServicesKey[] = "allServices";
const char kGooglePlusServiceKey[] = "googleme";

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace


// static
bool SigninManager::AreSigninCookiesAllowed(Profile* profile) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile);
  return cookie_settings &&
      cookie_settings->IsSettingCookieAllowed(GURL(kGoogleAccountsUrl),
                                              GURL(kGoogleAccountsUrl));
}

SigninManager::SigninManager()
    : profile_(NULL),
      had_two_factor_error_(false),
      type_(SIGNIN_TYPE_NONE) {
}

SigninManager::~SigninManager() {}

void SigninManager::Initialize(Profile* profile) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  profile_ = profile;
  PrefService* local_state = g_browser_process->local_state();
  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(prefs::kGoogleServicesUsernamePattern,
                                    this);
  }

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService))
    profile->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);

  std::string user = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  if (!user.empty())
    SetAuthenticatedUsername(user);
  // TokenService can be null for unit tests.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (token_service) {
    token_service->Initialize(GaiaConstants::kChromeSource, profile_);
    if (!authenticated_username_.empty()) {
      token_service->LoadTokensFromDB();
    }
  }
  if (!user.empty() && !IsAllowedUsername(user)) {
    // User is signed in, but the username is invalid - the administrator must
    // have changed the policy since the last signin, so sign out the user.
    SignOut();
  }
}

bool SigninManager::IsInitialized() const {
  return profile_ != NULL;
}

bool SigninManager::IsAllowedUsername(const std::string& username) const {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return true; // In a unit test with no local state - all names are allowed.

  std::string pattern = local_state->GetString(
      prefs::kGoogleServicesUsernamePattern);
  if (pattern.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  if (pattern[0] == '*')
    pattern.insert(0, ".");

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  string16 pattern16 = UTF8ToUTF16(pattern);
  const icu::UnicodeString icu_pattern(pattern16.data(), pattern16.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << pattern << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  string16 username16 = UTF8ToUTF16(username);
  icu::UnicodeString icu_input(username16.data(), username16.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

void SigninManager::CleanupNotificationRegistration() {
#if !defined(OS_CHROMEOS)
  content::Source<TokenService> token_service(
      TokenServiceFactory::GetForProfile(profile_));
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

bool SigninManager::PrepareForSignin(SigninType type,
                                     const std::string& username,
                                     const std::string& password) {
  DCHECK(possibly_invalid_username_.empty() ||
         possibly_invalid_username_ == username);
  DCHECK(!username.empty());

  if (!IsAllowedUsername(username)) {
    // Account is not allowed by admin policy.
    HandleAuthError(GoogleServiceAuthError(
        GoogleServiceAuthError::ACCOUNT_DISABLED), true);
    return false;
  }

  // This attempt is either 1) the user trying to establish initial sync, or
  // 2) trying to refresh credentials for an existing username.  If it is 2, we
  // need to try again, but take care to leave state around tracking that the
  // user has successfully signed in once before with this username, so that on
  // restart we don't think sync setup has never completed.
  ClearTransientSigninData();
  type_ = type;
  possibly_invalid_username_.assign(username);
  password_.assign(password);

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  return true;
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(authenticated_username_.empty() ||
         username == authenticated_username_);

  if (!PrepareForSignin(SIGNIN_TYPE_CLIENT_LOGIN, username, password))
    return;

  client_login_->StartClientLogin(username,
                                  password,
                                  "",
                                  login_token,
                                  login_captcha,
                                  GaiaAuthFetcher::HostedAccountsNotAllowed);

  // Register for token availability.  The signin manager will pre-login the
  // user when the GAIA service token is ready for use.  Only do this if we
  // are not running in ChomiumOS, since it handles pre-login itself, and if
  // cookies are not disabled for Google accounts.
#if !defined(OS_CHROMEOS)
  if (AreSigninCookiesAllowed(profile_)) {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));
  }
#endif
}

void SigninManager::ProvideSecondFactorAccessCode(
    const std::string& access_code) {
  DCHECK(!possibly_invalid_username_.empty() && !password_.empty() &&
      last_result_.data.empty());
  DCHECK(type_ == SIGNIN_TYPE_CLIENT_LOGIN);

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

void SigninManager::StartSignInWithCredentials(const std::string& session_index,
                                               const std::string& username,
                                               const std::string& password) {
  DCHECK(authenticated_username_.empty());

  if (!PrepareForSignin(SIGNIN_TYPE_WITH_CREDENTIALS, username, password))
    return;

  // This function starts with the current state of the web session's cookie
  // jar and mints a new ClientLogin-style SID/LSID pair.  This involves going
  // throug the follow process or requests to GAIA and LSO:
  //
  // - call /o/oauth2/programmatic_auth with the returned token to get oauth2
  //   access and refresh tokens
  // - call /accounts/OAuthLogin with the oauth2 access token and get SID/LSID
  //   pair for use by the token service
  //
  // The resulting SID/LSID can then be used just as if
  // client_login_->StartClientLogin() had completed successfully.
  client_login_->StartCookieForOAuthLoginTokenExchange(session_index);
}

void SigninManager::StartSignInWithOAuth(const std::string& username,
                                         const std::string& password) {
  DCHECK(authenticated_username_.empty());

  if (!PrepareForSignin(SIGNIN_TYPE_CLIENT_OAUTH, username, password))
    return;

  std::vector<std::string> scopes;
  scopes.push_back(GaiaUrls::GetInstance()->oauth1_login_scope());
  const std::string& locale = g_browser_process->GetApplicationLocale();

  client_login_->StartClientOAuth(username, password, scopes, "", locale);

  // Register for token availability.  The signin manager will pre-login the
  // user when the GAIA service token is ready for use.  Only do this if we
  // are not running in ChomiumOS, since it handles pre-login itself, and if
  // cookies are not disabled for Google accounts.
#if !defined(OS_CHROMEOS)
  if (AreSigninCookiesAllowed(profile_)) {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));
  }
#endif
}

void SigninManager::ProvideOAuthChallengeResponse(
    GoogleServiceAuthError::State type,
    const std::string& token,
    const std::string& solution) {
  DCHECK(!possibly_invalid_username_.empty() && !password_.empty());
  DCHECK(type_ == SIGNIN_TYPE_CLIENT_OAUTH);

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  client_login_->StartClientOAuthChallengeResponse(type, token, solution);
}

void SigninManager::ClearTransientSigninData() {
  DCHECK(IsInitialized());

  CleanupNotificationRegistration();
  client_login_.reset();
  last_result_ = ClientLoginResult();
  possibly_invalid_username_.clear();
  password_.clear();
  had_two_factor_error_ = false;
  type_ = SIGNIN_TYPE_NONE;
}

void SigninManager::HandleAuthError(const GoogleServiceAuthError& error,
                                    bool clear_transient_data) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceAuthError>(&error));

  // In some cases, the user should not be signed out.  For example, the failure
  // may be due to a captcha or OTP challenge.  In these cases, the transient
  // data must be kept to properly handle the follow up.
  if (clear_transient_data)
    ClearTransientSigninData();
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
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->ResetCredentialsInMemory();
  token_service->EraseTokensFromDB();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_username_.empty();
}

void SigninManager::OnGetUserInfoKeyNotFound(const std::string& key) {
  DCHECK(key == kGetInfoEmailKey);
  LOG(ERROR) << "Account is not associated with a valid email address. "
             << "Login failed.";
  OnClientLoginFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void SigninManager::OnClientLoginSuccess(const ClientLoginResult& result) {
  last_result_ = result;
  // Make a request for the canonical email address and services.
  client_login_->StartGetUserInfo(result.lsid);
}

void SigninManager::OnClientLoginFailure(const GoogleServiceAuthError& error) {
  // If we got a bad ASP, prompt for an ASP again by forcing another TWO_FACTOR
  // error.  This function does not call HandleAuthError() because dealing
  // with TWO_FACTOR errors needs special handling: we don't want to clear the
  // transient signin data in such error cases.
  bool invalid_gaia = error.state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS;

  GoogleServiceAuthError current_error =
      (invalid_gaia && had_two_factor_error_) ?
      GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR) : error;

  if (current_error.state() == GoogleServiceAuthError::TWO_FACTOR)
    had_two_factor_error_ = true;

  HandleAuthError(current_error, !had_two_factor_error_);
}

void SigninManager::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  DVLOG(1) << "SigninManager::OnClientOAuthSuccess access_token="
           << result.access_token;

  switch (type_) {
    case SIGNIN_TYPE_CLIENT_OAUTH:
    case SIGNIN_TYPE_WITH_CREDENTIALS:
      client_login_->StartOAuthLogin(result.access_token,
                                     GaiaConstants::kGaiaService);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SigninManager::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  bool clear_transient_data = true;
  if (type_ == SIGNIN_TYPE_CLIENT_OAUTH) {
    // If the error is a challenge (captcha or 2-factor), then don't sign out.
    clear_transient_data =
        error.state() != GoogleServiceAuthError::TWO_FACTOR &&
        error.state() != GoogleServiceAuthError::CAPTCHA_REQUIRED;
  } else {
    LOG(WARNING) << "SigninManager::OnClientOAuthFailure";
  }
  HandleAuthError(error, clear_transient_data);
}

void SigninManager::OnGetUserInfoSuccess(const UserInfoMap& data) {
  UserInfoMap::const_iterator email_iter = data.find(kGetInfoEmailKey);
  if (email_iter == data.end()) {
    OnGetUserInfoKeyNotFound(kGetInfoEmailKey);
    return;
  } else {
    DCHECK(email_iter->first == kGetInfoEmailKey);
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

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(last_result_);
  DCHECK(token_service->AreCredentialsValid());
  token_service->StartFetchingTokens();
}

void SigninManager::OnGetUserInfoFailure(const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Unable to retreive the canonical email address. Login failed.";
  // REVIEW: why does this call OnClientLoginFailure?
  OnClientLoginFailure(error);
}

void SigninManager::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED:
      DCHECK(*content::Details<std::string>(details).ptr() ==
             prefs::kGoogleServicesUsernamePattern);
      if (!authenticated_username_.empty() &&
          !IsAllowedUsername(authenticated_username_)) {
        // Signed in user is invalid according to the current policy so sign
        // the user out.
        SignOut();
      }
      break;

#if !defined(OS_CHROMEOS)
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      TokenService::TokenAvailableDetails* tok_details =
          content::Details<TokenService::TokenAvailableDetails>(
              details).ptr();

      // If a GAIA service token has become available, use it to pre-login the
      // user to other services that depend on GAIA credentials.
      if (tok_details->service() == GaiaConstants::kGaiaService) {
        if (client_login_.get() == NULL) {
          client_login_.reset(
              new GaiaAuthFetcher(this,
                                  GaiaConstants::kChromeSource,
                                  profile_->GetRequestContext()));
        }

        client_login_->StartMergeSession(tok_details->token());

        // We only want to do this once per sign-in.
        CleanupNotificationRegistration();
      }
      break;
    }
#endif
    default:
      NOTREACHED();
  }
}

