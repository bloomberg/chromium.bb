// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
#include "chrome/browser/policy/user_policy_signin_service.h"
#include "chrome/browser/policy/user_policy_signin_service_factory.h"
#endif

using namespace signin_internals_util;

using content::BrowserThread;

namespace {

const char kGetInfoDisplayEmailKey[] = "displayEmail";
const char kGetInfoEmailKey[] = "email";

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

// This class fetches GAIA cookie on IO thread on behalf of SigninManager which
// only lives on the UI thread.
class SigninManagerCookieHelper
    : public base::RefCountedThreadSafe<SigninManagerCookieHelper> {
 public:
  explicit SigninManagerCookieHelper(
      net::URLRequestContextGetter* request_context_getter);

  // Starts the fetching process, which will notify its completion via
  // callback.
  void StartFetchingGaiaCookiesOnUIThread(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

 private:
  friend class base::RefCountedThreadSafe<SigninManagerCookieHelper>;
  ~SigninManagerCookieHelper();

  // Fetch the GAIA cookies. This must be called in the IO thread.
  void FetchGaiaCookiesOnIOThread();

  // Callback for fetching cookies. This must be called in the IO thread.
  void OnGaiaCookiesFetched(const net::CookieList& cookies);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyOnUIThread(const net::CookieList& cookies);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  // This only mutates on the UI thread.
  base::Callback<void(const net::CookieList& cookies)> completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerCookieHelper);
};

SigninManagerCookieHelper::SigninManagerCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SigninManagerCookieHelper::~SigninManagerCookieHelper() {
}

void SigninManagerCookieHelper::StartFetchingGaiaCookiesOnUIThread(
    const base::Callback<void(const net::CookieList& cookies)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());

  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::FetchGaiaCookiesOnIOThread, this));
}

void SigninManagerCookieHelper::FetchGaiaCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<net::CookieMonster> cookie_monster =
      request_context_getter_->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  if (cookie_monster) {
    cookie_monster->GetAllCookiesForURLAsync(
        GURL(GaiaUrls::GetInstance()->gaia_origin_url()),
        base::Bind(&SigninManagerCookieHelper::OnGaiaCookiesFetched, this));
  } else {
    OnGaiaCookiesFetched(net::CookieList());
  }
}

void SigninManagerCookieHelper::OnGaiaCookiesFetched(
    const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::NotifyOnUIThread, this, cookies));
}

void SigninManagerCookieHelper::NotifyOnUIThread(
    const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ResetAndReturn(&completion_callback_).Run(cookies);
}

// static
bool SigninManager::AreSigninCookiesAllowed(Profile* profile) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile);
  return AreSigninCookiesAllowed(cookie_settings);
}

// static
bool SigninManager::AreSigninCookiesAllowed(CookieSettings* cookie_settings) {
  return cookie_settings &&
      cookie_settings->IsSettingCookieAllowed(GURL(kGoogleAccountsUrl),
                                              GURL(kGoogleAccountsUrl));
}

// static
bool SigninManager::IsAllowedUsername(const std::string& username,
                                      const std::string& policy) {
  if (policy.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  string16 pattern = UTF8ToUTF16(policy);
  if (pattern[0] == L'*')
    pattern.insert(pattern.begin(), L'.');

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
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

SigninManager::SigninManager()
    : profile_(NULL),
      prohibit_signout_(false),
      had_two_factor_error_(false),
      type_(SIGNIN_TYPE_NONE),
      weak_pointer_factory_(this) {
}

SigninManager::~SigninManager() {
  DCHECK(!signin_global_error_.get()) <<
      "SigninManager::Initialize called but not SigninManager::Shutdown";
}

void SigninManager::Initialize(Profile* profile) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  profile_ = profile;
  signin_global_error_.reset(new SigninGlobalError(this, profile));
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      signin_global_error_.get());
  PrefService* local_state = g_browser_process->local_state();
  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&SigninManager::OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(prefs::kSigninAllowed, profile_->GetPrefs(),
                       base::Bind(&SigninManager::OnSigninAllowedPrefChanged,
                                  base::Unretained(this)));

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
    // ChromeOS will kick off TokenService::LoadTokensFromDB from
    // OAuthLoginManager once the rest of the Profile is fully initialized.
    // Starting it from here would cause OAuthLoginManager mismatch the origin
    // of OAuth2 tokens.
#if !defined(OS_CHROMEOS)
    if (!authenticated_username_.empty()) {
      token_service->LoadTokensFromDB();
    }
#endif
  }
  if ((!user.empty() && !IsAllowedUsername(user)) || !IsSigninAllowed()) {
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
  return IsAllowedUsername(username, pattern);
}

bool SigninManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

// static
bool SigninManager::IsSigninAllowedOnIOThread(ProfileIOData* io_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return io_data->signin_allowed()->GetValue();
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

const std::string& SigninManager::GetAuthenticatedUsername() const {
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

  NotifyDiagnosticsObservers(USERNAME, username);

  // Go ahead and update the last signed in username here as well. Once a
  // user is signed in the two preferences should match. Doing it here as
  // opposed to on signin allows us to catch the upgrade scenario.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername, username);
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
    case SIGNIN_TYPE_CLIENT_OAUTH:
      return "Client OAuth";
  }

  NOTREACHED();
  return "";
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

  NotifyDiagnosticsObservers(SIGNIN_TYPE, SigninTypeToString(type));
  return true;
}

// Users must always sign out before they sign in again.
void SigninManager::StartSignIn(const std::string& username,
                                const std::string& password,
                                const std::string& login_token,
                                const std::string& login_captcha) {
  DCHECK(authenticated_username_.empty() ||
         gaia::AreEmailsSame(username, authenticated_username_));

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
  DCHECK(authenticated_username_.empty() ||
         gaia::AreEmailsSame(username, authenticated_username_));

  if (!PrepareForSignin(SIGNIN_TYPE_WITH_CREDENTIALS, username, password))
    return;

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
#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  policy_client_.reset();
#endif
  last_result_ = ClientLoginResult();
  possibly_invalid_username_.clear();
  password_.clear();
  had_two_factor_error_ = false;
  type_ = SIGNIN_TYPE_NONE;
  temp_oauth_login_tokens_ = ClientOAuthResult();
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
  if (prohibit_signout_) {
    DVLOG(1) << "Ignoring attempt to sign out while signout is prohibited";
    return;
  }
  if (authenticated_username_.empty() && !client_login_.get()) {
    // Clean up our transient data and exit if we aren't signed in (or in the
    // process of signing in). This avoids a perf regression from clearing out
    // the TokenDB if SignOut() is invoked on startup to clean up any
    // incomplete previous signin attempts.
    ClearTransientSigninData();
    return;
  }

  GoogleServiceSignoutDetails details(authenticated_username_);

  ClearTransientSigninData();
  authenticated_username_.clear();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);

  // Erase (now) stale information from AboutSigninInternals.
  NotifyDiagnosticsObservers(USERNAME, "");
  NotifyDiagnosticsObservers(LSID, "");
  NotifyDiagnosticsObservers(
      signin_internals_util::SID, "");

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSignoutDetails>(&details));
  token_service->ResetCredentialsInMemory();
  token_service->EraseTokensFromDB();
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_username_.empty();
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
    case SIGNIN_TYPE_CLIENT_OAUTH:
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

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  // TODO(atwilson): Move this code out to OneClickSignin instead of having
  // it embedded in SigninManager - we don't want UI logic in SigninManager.
  // If this is a new signin (authenticated_username_ is not set) and we have
  // an OAuth token, try loading policy for this user now, before any signed in
  // services are initialized. If there's no oauth token (the user is using the
  // old ClientLogin flow) then policy will get loaded once the TokenService
  // finishes initializing (not ideal, but it's a reasonable fallback).
  if (authenticated_username_.empty() &&
      !temp_oauth_login_tokens_.refresh_token.empty()) {
    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->RegisterPolicyClient(
        possibly_invalid_username_,
        temp_oauth_login_tokens_.refresh_token,
        base::Bind(&SigninManager::OnRegisteredForPolicy,
                   weak_pointer_factory_.GetWeakPtr()));
    return;
  }
#endif

  // Not waiting for policy load - just complete signin directly.
  CompleteSigninAfterPolicyLoad();
}

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
void SigninManager::OnRegisteredForPolicy(
    scoped_ptr<policy::CloudPolicyClient> client) {
  // If there's no token for the user (no policy) just finish signing in.
  if (!client.get()) {
    DVLOG(1) << "Policy registration failed";
    CompleteSigninAfterPolicyLoad();
    return;
  }

  // Stash away a copy of our CloudPolicyClient (should not already have one).
  DCHECK(!policy_client_);
  policy_client_.swap(client);

  DVLOG(1) << "Policy registration succeeded: dm_token="
           << policy_client_->dm_token();

  // Allow user to create a new profile before continuing with sign-in.
  ProfileSigninConfirmationDialog::ShowDialog(
      profile_,
      possibly_invalid_username_,
      base::Bind(&SigninManager::SignOut,
                 weak_pointer_factory_.GetWeakPtr()),
      base::Bind(&SigninManager::TransferCredentialsToNewProfile,
                 weak_pointer_factory_.GetWeakPtr()),
      base::Bind(&SigninManager::LoadPolicyWithCachedClient,
                 weak_pointer_factory_.GetWeakPtr()));
}

void SigninManager::LoadPolicyWithCachedClient() {
  DCHECK(policy_client_);
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      policy_client_.Pass(),
      base::Bind(&SigninManager::OnPolicyFetchComplete,
                 weak_pointer_factory_.GetWeakPtr()));
}

void SigninManager::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call SignOut() here
  // instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
  CompleteSigninAfterPolicyLoad();
}

void SigninManager::TransferCredentialsToNewProfile() {
  DCHECK(!possibly_invalid_username_.empty());
  DCHECK(policy_client_);
  // Create a new profile and have it call back when done so we can inject our
  // signin credentials.
  ProfileManager::CreateMultiProfileAsync(
      UTF8ToUTF16(possibly_invalid_username_),
      UTF8ToUTF16(ProfileInfoCache::GetDefaultAvatarIconUrl(1)),
      base::Bind(&SigninManager::CompleteSigninForNewProfile,
                 weak_pointer_factory_.GetWeakPtr()),
      chrome::GetActiveDesktop(),
      false);
}

void SigninManager::CompleteSigninForNewProfile(
    Profile* profile,
    Profile::CreateStatus status) {
  DCHECK_NE(profile_, profile);
  // TODO(atwilson): On error, unregister the client to release the DMToken.
  if (status == Profile::CREATE_STATUS_FAIL) {
    NOTREACHED() << "Error creating new profile";
    SignOut();
    return;
  }

  // Wait until the profile is initialized before we transfer credentials.
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    DCHECK(!possibly_invalid_username_.empty());
    DCHECK(policy_client_);
    // Sign in to the just-created profile and fetch policy for it.
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    DCHECK(signin_manager);
    signin_manager->possibly_invalid_username_ = possibly_invalid_username_;
    signin_manager->last_result_ = last_result_;
    signin_manager->temp_oauth_login_tokens_ = temp_oauth_login_tokens_;
    signin_manager->policy_client_.reset(policy_client_.release());
    signin_manager->LoadPolicyWithCachedClient();
    // Allow sync to start up if it is not overridden by policy.
    browser_sync::SyncPrefs prefs(profile->GetPrefs());
    prefs.SetSyncSetupCompleted();

    // We've transferred our credentials to the new profile - sign out.
    SignOut();
  }
}
#endif

void SigninManager::CompleteSigninAfterPolicyLoad() {
  DCHECK(!possibly_invalid_username_.empty());
  SetAuthenticatedUsername(possibly_invalid_username_);
  possibly_invalid_username_.clear();
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  authenticated_username_);

  GoogleServiceSigninSuccessDetails details(authenticated_username_,
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
#if !defined(OS_CHROMEOS)
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
#endif
    default:
      NOTREACHED();
  }
}

void SigninManager::Shutdown() {
  if (signin_global_error_.get()) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
        signin_global_error_.get());
    signin_global_error_.reset();
  }
}

void SigninManager::ProhibitSignout() {
  prohibit_signout_ = true;
}

bool SigninManager::IsSignoutProhibited() const {
  return prohibit_signout_;
}

void SigninManager::OnGoogleServicesUsernamePatternChanged() {
  if (!authenticated_username_.empty() &&
      !IsAllowedUsername(authenticated_username_)) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut();
  }
}

void SigninManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed())
    SignOut();
}

void SigninManager::AddSigninDiagnosticsObserver(
    SigninDiagnosticsObserver* observer) {
  signin_diagnostics_observers_.AddObserver(observer);
}

void SigninManager::RemoveSigninDiagnosticsObserver(
    SigninDiagnosticsObserver* observer) {
  signin_diagnostics_observers_.RemoveObserver(observer);
}

void SigninManager::NotifyDiagnosticsObservers(
    const UntimedSigninStatusField& field,
    const std::string& value) {
  FOR_EACH_OBSERVER(SigninDiagnosticsObserver,
                    signin_diagnostics_observers_,
                    NotifySigninValueChanged(field, value));
}

void SigninManager::NotifyDiagnosticsObservers(
    const TimedSigninStatusField& field,
    const std::string& value) {
  FOR_EACH_OBSERVER(SigninDiagnosticsObserver,
                    signin_diagnostics_observers_,
                    NotifySigninValueChanged(field, value));
}
