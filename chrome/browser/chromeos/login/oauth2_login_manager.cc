// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth2_login_manager.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Prefs registered only for migration purposes.
const char kOAuth1Token[] = "settings.account.oauth1_token";
const char kOAuth1Secret[] = "settings.account.oauth1_secret";

}  // namespace

namespace chromeos {

OAuth2LoginManager::OAuth2LoginManager(OAuthLoginManager::Delegate* delegate)
    : OAuthLoginManager(delegate),
      loading_reported_(false) {
}

OAuth2LoginManager::~OAuth2LoginManager() {
}

void OAuth2LoginManager::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kOAuth1Token,
                               "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(kOAuth1Secret,
                               "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void OAuth2LoginManager::RestoreSession(
    Profile* user_profile,
    net::URLRequestContextGetter* auth_request_context,
    SessionRestoreStrategy restore_strategy,
    const std::string& oauth2_refresh_token,
    const std::string& auth_code) {
  user_profile_ = user_profile;
  auth_request_context_ = auth_request_context;
  state_ = OAuthLoginManager::SESSION_RESTORE_IN_PROGRESS;
  restore_strategy_ = restore_strategy;
  refresh_token_ = oauth2_refresh_token;
  auth_code_ = auth_code;

  // TODO(zelidrag): Remove eventually the next line in some future milestone.
  RemoveLegacyTokens();

  // Reuse the access token fetched by the OAuth2PolicyFetcher, if it was
  // used to fetch policies before Profile creation.
  if (oauth2_policy_fetcher_.get() &&
      oauth2_policy_fetcher_->has_oauth2_tokens()) {
    VLOG(1) << "Resuming profile creation after fetching policy token";
    // We already have tokens, no need to get them from the cookie jar again.
    if (restore_strategy_ == RESTORE_FROM_COOKIE_JAR)
      restore_strategy_ = RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;

    StoreOAuth2Tokens(oauth2_policy_fetcher_->oauth2_tokens());
  }

  ContinueSessionRestore();
}

void OAuth2LoginManager::ContinueSessionRestore() {
  if (restore_strategy_ == RESTORE_FROM_COOKIE_JAR ||
      restore_strategy_ == RESTORE_FROM_AUTH_CODE) {
    FetchOAuth2Tokens();
    return;
  }

  // Save passed OAuth2 refresh token.
  if (restore_strategy_ == RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN) {
    DCHECK(!refresh_token_.empty());
    restore_strategy_ = RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
    GaiaAuthConsumer::ClientOAuthResult oauth2_tokens;
    oauth2_tokens.refresh_token = refresh_token_;
    StoreOAuth2Tokens(oauth2_tokens);
    return;
  }

  DCHECK(restore_strategy_ == RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN);
  LoadAndVerifyOAuth2Tokens();
}

void OAuth2LoginManager::RestorePolicyTokens(
    net::URLRequestContextGetter* auth_request_context) {
  oauth2_policy_fetcher_.reset(
      new OAuth2PolicyFetcher(auth_request_context,
                              g_browser_process->system_request_context()));
  oauth2_policy_fetcher_->Start();
}

void OAuth2LoginManager::Stop() {
  oauth2_token_fetcher_.reset();
  login_verifier_.reset();
}

TokenService* OAuth2LoginManager::SetupTokenService() {
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile_);
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                   content::Source<TokenService>(token_service));
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));
  }
  return token_service;
}

void OAuth2LoginManager::RemoveLegacyTokens() {
  PrefService* prefs = user_profile_->GetPrefs();
  prefs->ClearPref(kOAuth1Token);
  prefs->ClearPref(kOAuth1Secret);
}

void OAuth2LoginManager::StoreOAuth2Tokens(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  TokenService* token_service = SetupTokenService();
  token_service->UpdateCredentialsWithOAuth2(oauth2_tokens);
}

void OAuth2LoginManager::LoadAndVerifyOAuth2Tokens() {
  // If we have no cookies, try to load saved OAuth2 token from TokenService.
  TokenService* token_service = SetupTokenService();
  token_service->Initialize(GaiaConstants::kChromeSource, user_profile_);
  token_service->LoadTokensFromDB();
}

void OAuth2LoginManager::FetchOAuth2Tokens() {
  DCHECK(auth_request_context_.get());
  // If we have authenticated cookie jar, get OAuth1 token first, then fetch
  // SID/LSID cookies through OAuthLogin call.
  if (restore_strategy_ == RESTORE_FROM_COOKIE_JAR) {
    oauth2_token_fetcher_.reset(
        new OAuth2TokenFetcher(this, auth_request_context_));
    oauth2_token_fetcher_->StartExchangeFromCookies();
  } else if (restore_strategy_ == RESTORE_FROM_AUTH_CODE) {
    DCHECK(!auth_code_.empty());
    oauth2_token_fetcher_.reset(
        new OAuth2TokenFetcher(this,
                               g_browser_process->system_request_context()));
    oauth2_token_fetcher_->StartExchangeFromAuthCode(auth_code_);
  } else {
    NOTREACHED();
  }
}

void OAuth2LoginManager::OnOAuth2TokensAvailable(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  LOG(INFO) << "OAuth2 tokens fetched";
  StoreOAuth2Tokens(oauth2_tokens);
}

void OAuth2LoginManager::OnOAuth2TokensFetchFailed() {
  LOG(ERROR) << "OAuth2 tokens fetch failed!";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      User::OAUTH2_TOKEN_STATUS_INVALID);
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_TOKEN_FETCH_FAILED,
                            SESSION_RESTORE_COUNT);
}

void OAuth2LoginManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile_);
  switch (type) {
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED: {
      refresh_token_ = token_service->GetOAuth2LoginRefreshToken();
      RestoreSessionCookies();
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      // This path should not kick in if are loading OAuth2 refresh token
      // from the TokenService.
      if (restore_strategy_ == RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN)
        return;

      TokenService::TokenAvailableDetails* token_details =
          content::Details<TokenService::TokenAvailableDetails>(
              details).ptr();
      if (token_details->service() ==
              GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        DCHECK(!login_verifier_.get());
        refresh_token_ = token_details->token();
        RestoreSessionCookies();
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void OAuth2LoginManager::RestoreSessionCookies() {
  if (refresh_token_.empty()) {
    LOG(ERROR) << "OAuth2 refresh token is empty!";
    state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
    UserManager::Get()->SaveUserOAuthStatus(
        UserManager::Get()->GetLoggedInUser()->email(),
        User::OAUTH2_TOKEN_STATUS_INVALID);
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                              SESSION_RESTORE_NO_REFRESH_TOKEN_FAILED,
                              SESSION_RESTORE_COUNT);
    return;
  }

  DCHECK(!login_verifier_.get());
  login_verifier_.reset(
      new OAuth2LoginVerifier(this,
                              g_browser_process->system_request_context(),
                              user_profile_->GetRequestContext()));
  login_verifier_->VerifyOAuth2RefreshToken(refresh_token_);
  FetchPolicyTokens();
}

void OAuth2LoginManager::FetchPolicyTokens() {
  DCHECK(!refresh_token_.empty());
  if (!oauth2_policy_fetcher_.get() || oauth2_policy_fetcher_->failed()) {
    // Trigger OAuth2 token fetch for user policy.
    oauth2_policy_fetcher_.reset(
        new OAuth2PolicyFetcher(g_browser_process->system_request_context(),
                                refresh_token_));
    oauth2_policy_fetcher_->Start();
  }
}
void OAuth2LoginManager::OnOAuthLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& gaia_credentials) {
  LOG(INFO) << "OAuth2 refresh token successfully exchanged for GAIA token.";
  StartTokenService(gaia_credentials);
}

void OAuth2LoginManager::OnOAuthLoginFailure() {
  LOG(ERROR) << "OAuth2 refresh token verification failed!";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      User::OAUTH2_TOKEN_STATUS_INVALID);
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_OAUTHLOGIN_FAILED,
                            SESSION_RESTORE_COUNT);
  delegate_->OnCompletedMergeSession();
}

void OAuth2LoginManager::OnSessionMergeSuccess() {
  LOG(INFO) << "OAuth2 refresh and/or GAIA token verification succeeded.";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      User::OAUTH2_TOKEN_STATUS_VALID);
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_SUCCESS,
                            SESSION_RESTORE_COUNT);
  delegate_->OnCompletedMergeSession();
}

void OAuth2LoginManager::OnSessionMergeFailure() {
  LOG(ERROR) << "OAuth2 refresh and GAIA token verification failed!";
  state_ = OAuthLoginManager::SESSION_RESTORE_DONE;
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      User::OAUTH2_TOKEN_STATUS_INVALID);
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_MERGE_SESSION_FAILED,
                            SESSION_RESTORE_COUNT);
  delegate_->OnCompletedMergeSession();
}

void OAuth2LoginManager::StartTokenService(
    const GaiaAuthConsumer::ClientLoginResult& gaia_credentials) {
  TokenService* token_service = SetupTokenService();
  token_service->UpdateCredentials(gaia_credentials);
  CompleteAuthentication();
}

}  // namespace chromeos
