// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth1_login_manager.h"

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

OAuth1LoginManager::OAuth1LoginManager(
    OAuthLoginManager::Delegate* delegate)
    : OAuthLoginManager(delegate) {
}

OAuth1LoginManager::~OAuth1LoginManager() {
}

void OAuth1LoginManager::RestoreSession(
    Profile* user_profile,
    net::URLRequestContextGetter* auth_request_context,
    bool restore_from_auth_cookies) {
  user_profile_ = user_profile;
  auth_request_context_ = auth_request_context;
  state_ = OAuthLoginManager::SESSION_RESTORE_IN_PROGRESS;

  // Reuse the access token fetched by the PolicyOAuthFetcher, if it was
  // used to fetch policies before Profile creation.
  if (policy_oauth_fetcher_.get() &&
      !policy_oauth_fetcher_->oauth1_token().empty()) {
    VLOG(1) << "Resuming profile creation after fetching policy token";
    oauth1_token_ = policy_oauth_fetcher_->oauth1_token();
    oauth1_secret_ = policy_oauth_fetcher_->oauth1_secret();
    StoreOAuth1Tokens();
    restore_from_auth_cookies = false;
  }
  restore_from_auth_cookies_ = restore_from_auth_cookies;
  ContinueSessionRestore();
}

void OAuth1LoginManager::ContinueSessionRestore() {
  // Have we even started session restore?
  if (!user_profile_)
    return;

  if (restore_from_auth_cookies_) {
    DCHECK(auth_request_context_.get());
    // If we don't have it, fetch OAuth1 access token.
    // Once we get that, we will kick off individual requests for OAuth2
    // tokens for all our services.
    // Use off-the-record profile that was used for this step. It should
    // already contain all needed cookies that will let us skip GAIA's user
    // authentication UI.
    oauth1_token_fetcher_.reset(
        new OAuth1TokenFetcher(this, auth_request_context_));
    oauth1_token_fetcher_->Start();
  } else if (ReadOAuth1Tokens()) {
    // Verify OAuth access token when we find it in the profile and always if
    // if we don't have cookies.
    // TODO(xiyuan): Change back to use authenticator to verify token when
    // we support Gaia in lock screen.
    VerifyOAuth1AccessToken();
  } else {
    UserManager::Get()->SaveUserOAuthStatus(
        UserManager::Get()->GetLoggedInUser()->email(),
        User::OAUTH1_TOKEN_STATUS_INVALID);
  }
}

void OAuth1LoginManager::RestorePolicyTokens(
    net::URLRequestContextGetter* auth_request_context) {
  DCHECK(!policy_oauth_fetcher_.get());
  policy_oauth_fetcher_.reset(
      new PolicyOAuthFetcher(auth_request_context));
  policy_oauth_fetcher_->Start();
}

void OAuth1LoginManager::Stop() {
  oauth1_token_fetcher_.reset();
  oauth1_login_verifier_.reset();
  state_ = OAuthLoginManager::SESSION_RESTORE_NOT_STARTED;
}

void OAuth1LoginManager::VerifyOAuth1AccessToken() {
  // Kick off verification of OAuth1 access token (via OAuthLogin), this should
  // let us fetch credentials that will be used to initialize sync engine.
  FetchCredentialsWithOAuth1();
  delegate_->OnFoundStoredTokens();
  FetchPolicyTokens();
}

void OAuth1LoginManager::FetchPolicyTokens() {
  if (!policy_oauth_fetcher_.get() || policy_oauth_fetcher_->failed()) {
    // Trigger oauth token fetch for user policy.
    policy_oauth_fetcher_.reset(
        new PolicyOAuthFetcher(g_browser_process->system_request_context(),
                               oauth1_token_,
                               oauth1_secret_));
    policy_oauth_fetcher_->Start();
  }
}

void OAuth1LoginManager::FetchCredentialsWithOAuth1() {
  oauth1_login_verifier_.reset(
      new OAuth1LoginVerifier(this,
                              user_profile_->GetRequestContext(),
                              oauth1_token_,
                              oauth1_secret_,
                              UserManager::Get()->GetLoggedInUser()->email()));
  oauth1_login_verifier_->StartOAuthVerification();
}

void OAuth1LoginManager::OnOAuth1AccessTokenAvailable(
    const std::string& token,
    const std::string& secret) {
  LOG(INFO) << "OAuth1 access token fetch succeeded.";
  oauth1_token_ = token;
  oauth1_secret_ = secret;
  StoreOAuth1Tokens();
  // Verify OAuth1 token by doing OAuthLogin and fetching credentials. If we
  // have just transfered auth cookies out of authenticated cookie jar, there
  // is no need to try to mint them from OAuth token again.
  VerifyOAuth1AccessToken();
}

void OAuth1LoginManager::OnOAuth1AccessTokenFetchFailed() {
  LOG(ERROR) << "OAuth1 access token fetch failed!";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  g_browser_process->browser_policy_connector()->RegisterForUserPolicy(
      EmptyString());
}

void OAuth1LoginManager::OnOAuth1VerificationSucceeded(
    const std::string& user_name, const std::string& sid,
    const std::string& lsid, const std::string& auth) {
  LOG(INFO) << "OAuth1 token verification succeeded";
  // Kick off sync engine.
  GaiaAuthConsumer::ClientLoginResult credentials(sid, lsid, auth,
                                                  std::string());
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile_);
  token_service->UpdateCredentials(credentials);
  CompleteAuthentication();
}

void OAuth1LoginManager::OnOAuth1VerificationFailed(
    const std::string& user_name) {
  LOG(ERROR) << "OAuth1 token verification failed!";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  UserManager::Get()->SaveUserOAuthStatus(user_name,
                                          User::OAUTH1_TOKEN_STATUS_INVALID);
}

bool OAuth1LoginManager::ReadOAuth1Tokens() {
  // Skip reading oauth token if user does not have a valid status.
  if (UserManager::Get()->IsUserLoggedIn() &&
      UserManager::Get()->GetLoggedInUser()->oauth_token_status() !=
          User::OAUTH1_TOKEN_STATUS_VALID) {
    return false;
  }

  PrefService* pref_service = user_profile_->GetPrefs();
  std::string encoded_token = pref_service->GetString(prefs::kOAuth1Token);
  std::string encoded_secret = pref_service->GetString(prefs::kOAuth1Secret);
  if (!encoded_token.length() || !encoded_secret.length())
    return false;

  std::string decoded_token =
      CrosLibrary::Get()->GetCertLibrary()->DecryptToken(encoded_token);
  std::string decoded_secret =
      CrosLibrary::Get()->GetCertLibrary()->DecryptToken(encoded_secret);

  if (!decoded_token.length() || !decoded_secret.length())
    return false;

  oauth1_token_ = decoded_token;
  oauth1_secret_ = decoded_secret;
  return true;
}

void OAuth1LoginManager::StoreOAuth1Tokens() {
  DCHECK(!oauth1_token_.empty());
  DCHECK(!oauth1_secret_.empty());
  // First store OAuth1 token + service for the current user profile...
  std::string encrypted_token =
      CrosLibrary::Get()->GetCertLibrary()->EncryptToken(oauth1_token_);
  std::string encrypted_secret =
      CrosLibrary::Get()->GetCertLibrary()->EncryptToken(oauth1_secret_);

  PrefService* pref_service = user_profile_->GetPrefs();
  User* user = UserManager::Get()->GetLoggedInUser();
  if (!encrypted_token.empty() && !encrypted_secret.empty()) {
    pref_service->SetString(prefs::kOAuth1Token, encrypted_token);
    pref_service->SetString(prefs::kOAuth1Secret, encrypted_secret);

    // ...then record the presence of valid OAuth token for this account in
    // local state as well.
    UserManager::Get()->SaveUserOAuthStatus(
        user->email(), User::OAUTH1_TOKEN_STATUS_VALID);
  } else {
    LOG(INFO) << "Failed to get OAuth1 token/secret encrypted.";
    // Set the OAuth status invalid so that the user will go through full
    // GAIA login next time.
    UserManager::Get()->SaveUserOAuthStatus(
        user->email(), User::OAUTH1_TOKEN_STATUS_INVALID);
  }
}

}  // namespace chromeos
