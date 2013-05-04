// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager_cookie_helper.h"
#include "chrome/browser/signin/signin_manager_delegate.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

using namespace signin_internals_util;

using content::BrowserThread;

namespace {

const char kGetInfoDisplayEmailKey[] = "displayEmail";
const char kGetInfoEmailKey[] = "email";

const int kInvalidProcessId = -1;

const char kChromiumSyncService[] = "service=chromiumsync";

}  // namespace

// Under the covers, we use a dummy chrome-extension ID to serve the purposes
// outlined in the .h file comment for this string.
const char* SigninManager::kChromeSigninEffectiveSite =
    "chrome-extension://acfccoigjajmmgbhpfbjnpckhjjegnih";

// static
bool SigninManager::IsWebBasedSigninFlowURL(const GURL& url) {
  GURL effective(kChromeSigninEffectiveSite);
  if (url.SchemeIs(effective.scheme().c_str()) &&
      url.host() == effective.host()) {
    return true;
  }

  GURL service_login(GaiaUrls::GetInstance()->service_login_url());
  if (url.GetOrigin() != service_login.GetOrigin())
    return false;

  // Any login UI URLs with signin=chromiumsync should be considered a web
  // URL (relies on GAIA keeping the "service=chromiumsync" query string
  // fragment present even when embedding inside a "continue" parameter).
  return net::UnescapeURLComponent(
      url.query(), net::UnescapeRule::URL_SPECIAL_CHARS)
          .find(kChromiumSyncService) != std::string::npos;
}

SigninManager::SigninManager(scoped_ptr<SigninManagerDelegate> delegate)
    : prohibit_signout_(false),
      had_two_factor_error_(false),
      type_(SIGNIN_TYPE_NONE),
      weak_pointer_factory_(this),
      signin_process_id_(kInvalidProcessId),
      delegate_(delegate.Pass()) {
}

void SigninManager::SetSigninProcess(int process_id) {
  if (process_id == signin_process_id_)
    return;
  DLOG_IF(WARNING, signin_process_id_ != kInvalidProcessId) <<
      "Replacing in-use signin process.";
  signin_process_id_ = process_id;
  const content::RenderProcessHost* process =
      content::RenderProcessHost::FromID(process_id);
  DCHECK(process);
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::Source<content::RenderProcessHost>(process));
}

bool SigninManager::IsSigninProcess(int process_id) const {
  return process_id == signin_process_id_;
}

bool SigninManager::HasSigninProcess() const {
  return signin_process_id_ != kInvalidProcessId;
}

SigninManager::~SigninManager() {
}

void SigninManager::InitTokenService() {
  SigninManagerBase::InitTokenService();
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (token_service && !GetAuthenticatedUsername().empty())
    token_service->LoadTokensFromDB();
}

void SigninManager::CleanupNotificationRegistration() {
  content::Source<TokenService> token_service(
      TokenServiceFactory::GetForProfile(profile_));
  if (registrar_.IsRegistered(this,
                              chrome::NOTIFICATION_TOKEN_AVAILABLE,
                              token_service)) {
    registrar_.Remove(this,
                      chrome::NOTIFICATION_TOKEN_AVAILABLE,
                      token_service);
  }
}

std::string SigninManager::SigninTypeToString(
    SigninManager::SigninType type) {
  switch (type) {
    case SIGNIN_TYPE_NONE:
      return "No Signin";
    case SIGNIN_TYPE_CLIENT_LOGIN:
      return "Client Login";
    case SIGNIN_TYPE_WITH_CREDENTIALS:
      return "Signin with credentials";
  }

  NOTREACHED();
  return std::string();
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
  RevokeOAuthLoginToken();
  ClearTransientSigninData();
  type_ = type;
  possibly_invalid_username_.assign(username);
  password_.assign(password);

  client_login_.reset(new GaiaAuthFetcher(this,
                                          GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));

  NotifyDiagnosticsObservers(SIGNIN_TYPE, SigninTypeToString(type));
  return true;
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(GetAuthenticatedUsername().empty() ||
         gaia::AreEmailsSame(username, GetAuthenticatedUsername()));

  if (!PrepareForSignin(SIGNIN_TYPE_CLIENT_LOGIN, username, password))
    return;

  client_login_->StartClientLogin(username,
                                  password,
                                  "",
                                  login_token,
                                  login_captcha,
                                  GaiaAuthFetcher::HostedAccountsNotAllowed);

  // Register for token availability.  The signin manager will pre-login the
  // user when the GAIA service token is ready for use.
  if (delegate_->AreSigninCookiesAllowed()) {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));
  }
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

void SigninManager::StartSignInWithCredentials(
    const std::string& session_index,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& callback) {
  DCHECK(GetAuthenticatedUsername().empty() ||
         gaia::AreEmailsSame(username, GetAuthenticatedUsername()));

  if (!PrepareForSignin(SIGNIN_TYPE_WITH_CREDENTIALS, username, password))
    return;

  // Store our callback.
  DCHECK(oauth_token_fetched_callback_.is_null());
  oauth_token_fetched_callback_ = callback;

  if (password.empty()) {
    // Chrome must verify the GAIA cookies first if auto sign-in is triggered
    // with no password provided. This is to protect Chrome against forged
    // GAIA cookies from a super-domain.
    VerifyGaiaCookiesBeforeSignIn(session_index);
  } else {
    // This function starts with the current state of the web session's cookie
    // jar and mints a new ClientLogin-style SID/LSID pair.  This involves going
    // through the follow process or requests to GAIA and LSO:
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
}

void SigninManager::VerifyGaiaCookiesBeforeSignIn(
    const std::string& session_index) {
  scoped_refptr<SigninManagerCookieHelper> cookie_helper(
      new SigninManagerCookieHelper(profile_->GetRequestContext()));
  cookie_helper->StartFetchingGaiaCookiesOnUIThread(
      base::Bind(&SigninManager::OnGaiaCookiesFetched,
                 weak_pointer_factory_.GetWeakPtr(), session_index));
}

void SigninManager::OnGaiaCookiesFetched(
    const std::string session_index, const net::CookieList& cookie_list) {
  net::CookieList::const_iterator it;
  bool success = false;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Make sure the LSID cookie is set on the GAIA host, instead of a super-
    // domain.
    if (it->Name() == "LSID") {
      if (it->IsHostCookie() && it->IsHttpOnly() && it->IsSecure()) {
        // Found a valid LSID cookie. Continue loop to make sure we don't have
        // invalid LSID cookies on any super-domain.
        success = true;
      } else {
        success = false;
        break;
      }
    }
  }

  if (success) {
    client_login_->StartCookieForOAuthLoginTokenExchange(session_index);
  } else {
    HandleAuthError(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS), true);
  }
}

void SigninManager::CopyCredentialsFrom(const SigninManager& source) {
  DCHECK_NE(this, &source);
  possibly_invalid_username_ = source.possibly_invalid_username_;
  last_result_ = source.last_result_;
  temp_oauth_login_tokens_ = source.temp_oauth_login_tokens_;
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
  temp_oauth_login_tokens_ = ClientOAuthResult();
  oauth_token_fetched_callback_.Reset();
}

void SigninManager::HandleAuthError(const GoogleServiceAuthError& error,
                                    bool clear_transient_data) {
  // In some cases, the user should not be signed out.  For example, the failure
  // may be due to a captcha or OTP challenge.  In these cases, the transient
  // data must be kept to properly handle the follow up. This routine clears
  // the data before sending out the notification so the SigninManager is no
  // longer in the AuthInProgress state when the notification goes out.
  if (clear_transient_data)
    ClearTransientSigninData();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceAuthError>(&error));
}

void SigninManager::SignOut() {
  DCHECK(IsInitialized());

  if (GetAuthenticatedUsername().empty()) {
    if (AuthInProgress()) {
      // If the user is in the process of signing in, then treat a call to
      // SignOut as a cancellation request.
      GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
      HandleAuthError(error, true);
    } else {
      // Clean up our transient data and exit if we aren't signed in.
      // This avoids a perf regression from clearing out the TokenDB if
      // SignOut() is invoked on startup to clean up any incomplete previous
      // signin attempts.
      ClearTransientSigninData();
    }
    return;
  }

  if (prohibit_signout_) {
    DVLOG(1) << "Ignoring attempt to sign out while signout is prohibited";
    return;
  }

  ClearTransientSigninData();
  RevokeOAuthLoginToken();
  SigninManagerBase::SignOut();
}

void SigninManager::RevokeOAuthLoginToken() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (token_service->HasOAuthLoginToken()) {
    revoke_token_fetcher_.reset(
        new GaiaAuthFetcher(this,
                            GaiaConstants::kChromeSource,
                            profile_->GetRequestContext()));
    revoke_token_fetcher_->StartRevokeOAuth2Token(
        token_service->GetOAuth2LoginRefreshToken());
  }
}

void SigninManager::OnOAuth2RevokeTokenCompleted() {
  revoke_token_fetcher_.reset(NULL);
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_username_.empty();
}

const std::string& SigninManager::GetUsernameForAuthInProgress() const {
  return possibly_invalid_username_;
}

void SigninManager::OnGetUserInfoKeyNotFound(const std::string& key) {
  DCHECK(key == kGetInfoDisplayEmailKey || key == kGetInfoEmailKey);
  LOG(ERROR) << "Account is not associated with a valid email address. "
             << "Login failed.";
  OnClientLoginFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void SigninManager::DisableOneClickSignIn(Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, false);
}

void SigninManager::OnClientLoginSuccess(const ClientLoginResult& result) {
  last_result_ = result;
  // Update signin_internals_
  NotifyDiagnosticsObservers(CLIENT_LOGIN_STATUS, "Successful");
  NotifyDiagnosticsObservers(LSID, result.lsid);
  NotifyDiagnosticsObservers(
      signin_internals_util::SID, result.sid);
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

  NotifyDiagnosticsObservers(CLIENT_LOGIN_STATUS, error.ToString());
  HandleAuthError(current_error, !had_two_factor_error_);
}

void SigninManager::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  DVLOG(1) << "SigninManager::OnClientOAuthSuccess access_token="
           << result.access_token;

  NotifyDiagnosticsObservers(OAUTH_LOGIN_STATUS, "Successful");

  switch (type_) {
    case SIGNIN_TYPE_WITH_CREDENTIALS:
      temp_oauth_login_tokens_ = result;
      client_login_->StartOAuthLogin(result.access_token,
                                     GaiaConstants::kGaiaService);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SigninManager::OnClientOAuthFailure(const GoogleServiceAuthError& error) {
  bool clear_transient_data = true;
  NotifyDiagnosticsObservers(OAUTH_LOGIN_STATUS, error.ToString());
  LOG(WARNING) << "SigninManager::OnClientOAuthFailure";
  HandleAuthError(error, clear_transient_data);
}

void SigninManager::OnGetUserInfoSuccess(const UserInfoMap& data) {
  NotifyDiagnosticsObservers(GET_USER_INFO_STATUS, "Successful");

  UserInfoMap::const_iterator email_iter = data.find(kGetInfoEmailKey);
  UserInfoMap::const_iterator display_email_iter =
      data.find(kGetInfoDisplayEmailKey);
  if (email_iter == data.end()) {
    OnGetUserInfoKeyNotFound(kGetInfoEmailKey);
    return;
  }
  if (display_email_iter == data.end()) {
    OnGetUserInfoKeyNotFound(kGetInfoDisplayEmailKey);
    return;
  }
  DCHECK(email_iter->first == kGetInfoEmailKey);
  DCHECK(display_email_iter->first == kGetInfoDisplayEmailKey);

  // When signing in with credentials, the possibly invalid name is the Gaia
  // display name. If the name returned by GetUserInfo does not match what is
  // expected, return an error.
  if (type_ == SIGNIN_TYPE_WITH_CREDENTIALS &&
      !gaia::AreEmailsSame(display_email_iter->second,
                           possibly_invalid_username_)) {
    OnGetUserInfoKeyNotFound(kGetInfoDisplayEmailKey);
    return;
  }

  possibly_invalid_username_ = email_iter->second;

  if (!oauth_token_fetched_callback_.is_null() &&
      !temp_oauth_login_tokens_.refresh_token.empty()) {
    oauth_token_fetched_callback_.Run(temp_oauth_login_tokens_.refresh_token);
  } else {
    // No oauth token or callback, so just complete our pending signin.
    CompletePendingSignin();
  }
}


void SigninManager::CompletePendingSignin() {
  DCHECK(!possibly_invalid_username_.empty());
  SetAuthenticatedUsername(possibly_invalid_username_);
  possibly_invalid_username_.clear();
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  GetAuthenticatedUsername());

  GoogleServiceSigninSuccessDetails details(GetAuthenticatedUsername(),
                                            password_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  password_.clear();  // Don't need it anymore.
  DisableOneClickSignIn(profile_);  // Don't ever offer again.

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(last_result_);
  DCHECK(token_service->AreCredentialsValid());
  token_service->StartFetchingTokens();

  // If we have oauth2 tokens, tell token service about them so it does not
  // need to fetch them again.
  if (!temp_oauth_login_tokens_.refresh_token.empty()) {
    token_service->UpdateCredentialsWithOAuth2(temp_oauth_login_tokens_);
    temp_oauth_login_tokens_ = ClientOAuthResult();
  }
}

void SigninManager::OnGetUserInfoFailure(const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Unable to retreive the canonical email address. Login failed.";
  NotifyDiagnosticsObservers(GET_USER_INFO_STATUS, error.ToString());
  // REVIEW: why does this call OnClientLoginFailure?
  OnClientLoginFailure(error);
}

void SigninManager::OnUbertokenSuccess(const std::string& token) {
  ubertoken_fetcher_.reset();
  if (client_login_.get() == NULL) {
    client_login_.reset(
        new GaiaAuthFetcher(this,
                            GaiaConstants::kChromeSource,
                            profile_->GetRequestContext()));
  }

  client_login_->StartMergeSession(token);
}

void SigninManager::OnUbertokenFailure(const GoogleServiceAuthError& error) {
  LOG(WARNING) << " Unable to login the user to the web: " << error.ToString();
  ubertoken_fetcher_.reset();
}

void SigninManager::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      TokenService::TokenAvailableDetails* tok_details =
          content::Details<TokenService::TokenAvailableDetails>(
              details).ptr();

      // If a GAIA service token has become available, use it to pre-login the
      // user to other services that depend on GAIA credentials.
      if (tok_details->service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        ubertoken_fetcher_.reset(new UbertokenFetcher(profile_, this));
        ubertoken_fetcher_->StartFetchingToken();

        // We only want to do this once per sign-in.
        CleanupNotificationRegistration();
      }
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      // It's possible we're listening to a "stale" renderer because it was
      // replaced with a new process by process-per-site. In either case,
      // stop listening to it, but only reset signin_process_id_ tracking
      // if this was from the current signin process.
      registrar_.Remove(this,
                        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                        source);
      if (signin_process_id_ ==
          content::Source<content::RenderProcessHost>(source)->GetID()) {
        signin_process_id_ = kInvalidProcessId;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void SigninManager::ProhibitSignout(bool prohibit_signout) {
  prohibit_signout_ = prohibit_signout;
}

bool SigninManager::IsSignoutProhibited() const {
  return prohibit_signout_;
}
