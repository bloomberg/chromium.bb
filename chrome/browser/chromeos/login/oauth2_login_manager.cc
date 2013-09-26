// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth2_login_manager.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

OAuth2LoginManager::OAuth2LoginManager(Profile* user_profile)
    : user_profile_(user_profile),
      restore_strategy_(RESTORE_FROM_COOKIE_JAR),
      state_(SESSION_RESTORE_NOT_STARTED),
      loading_reported_(false) {
  ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_)->
      AddObserver(this);
}

OAuth2LoginManager::~OAuth2LoginManager() {
}

void OAuth2LoginManager::AddObserver(OAuth2LoginManager::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OAuth2LoginManager::RemoveObserver(
    OAuth2LoginManager::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OAuth2LoginManager::RestoreSession(
    net::URLRequestContextGetter* auth_request_context,
    SessionRestoreStrategy restore_strategy,
    const std::string& oauth2_refresh_token,
    const std::string& auth_code) {
  DCHECK(user_profile_);
  auth_request_context_ = auth_request_context;
  restore_strategy_ = restore_strategy;
  refresh_token_ = oauth2_refresh_token;
  auth_code_ = auth_code;
  session_restore_start_ = base::Time::Now();
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_PREPARING);
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

void OAuth2LoginManager::Stop() {
  oauth2_token_fetcher_.reset();
  login_verifier_.reset();
}

bool OAuth2LoginManager::ShouldBlockTabLoading() {
  return state_ == SESSION_RESTORE_PREPARING ||
         state_ == SESSION_RESTORE_IN_PROGRESS;
}

void OAuth2LoginManager::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (state_ == SESSION_RESTORE_NOT_STARTED)
    return;

  // TODO(fgorski): Once ProfileOAuth2TokenService supports multi-login, make
  // sure to restore session cookies in the context of the correct account_id.
  LOG(INFO) << "OnRefreshTokenAvailable";
  RestoreSessionCookies();
}

TokenService* OAuth2LoginManager::SetupTokenService() {
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile_);
  return token_service;
}

void OAuth2LoginManager::StoreOAuth2Tokens(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  TokenService* token_service = SetupTokenService();
  token_service->UpdateCredentialsWithOAuth2(oauth2_tokens);

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnNewRefreshTokenAvaiable(user_profile_));
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
        new OAuth2TokenFetcher(this, auth_request_context_.get()));
    oauth2_token_fetcher_->StartExchangeFromCookies();
  } else if (restore_strategy_ == RESTORE_FROM_AUTH_CODE) {
    DCHECK(!auth_code_.empty());
    oauth2_token_fetcher_.reset(
        new OAuth2TokenFetcher(this,
                               g_browser_process->system_request_context()));
    oauth2_token_fetcher_->StartExchangeFromAuthCode(auth_code_);
  } else {
    NOTREACHED();
    SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_FAILED);
  }
}

void OAuth2LoginManager::OnOAuth2TokensAvailable(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  LOG(INFO) << "OAuth2 tokens fetched";
  StoreOAuth2Tokens(oauth2_tokens);
}

void OAuth2LoginManager::OnOAuth2TokensFetchFailed() {
  LOG(ERROR) << "OAuth2 tokens fetch failed!";
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_TOKEN_FETCH_FAILED,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::RestoreSessionCookies() {
  DCHECK(!login_verifier_.get());
  SetSessionRestoreState(SESSION_RESTORE_IN_PROGRESS);
  login_verifier_.reset(
      new OAuth2LoginVerifier(this,
                              g_browser_process->system_request_context(),
                              user_profile_->GetRequestContext()));
  login_verifier_->VerifyProfileTokens(user_profile_);
}

void OAuth2LoginManager::Shutdown() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_)->
      RemoveObserver(this);
  login_verifier_.reset();
  oauth2_token_fetcher_.reset();
}

void OAuth2LoginManager::OnOAuthLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& gaia_credentials) {
  LOG(INFO) << "OAuth2 refresh token successfully exchanged for GAIA token.";
  StartTokenService(gaia_credentials);
}

void OAuth2LoginManager::OnOAuthLoginFailure() {
  LOG(ERROR) << "OAuth2 refresh token verification failed!";
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_OAUTHLOGIN_FAILED,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::OnSessionMergeSuccess() {
  LOG(INFO) << "OAuth2 refresh and/or GAIA token verification succeeded.";
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_SUCCESS,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_DONE);
}

void OAuth2LoginManager::OnSessionMergeFailure() {
  LOG(ERROR) << "OAuth2 refresh and GAIA token verification failed!";
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore",
                            SESSION_RESTORE_MERGE_SESSION_FAILED,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::StartTokenService(
    const GaiaAuthConsumer::ClientLoginResult& gaia_credentials) {
  TokenService* token_service = SetupTokenService();
  token_service->UpdateCredentials(gaia_credentials);

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnSessionAuthenticated(user_profile_));

  if (token_service->AreCredentialsValid())
    token_service->StartFetchingTokens();
}

void OAuth2LoginManager::SetSessionRestoreState(
    OAuth2LoginManager::SessionRestoreState state) {
  if (state_ == state)
    return;

  state_ = state;
  if (state == OAuth2LoginManager::SESSION_RESTORE_FAILED) {
    UMA_HISTOGRAM_TIMES("OAuth2Login.SessionRestoreTimeToFailure",
                        base::Time::Now() - session_restore_start_);
  } else if (state == OAuth2LoginManager::SESSION_RESTORE_DONE) {
    UMA_HISTOGRAM_TIMES("OAuth2Login.SessionRestoreTimeToSuccess",
                        base::Time::Now() - session_restore_start_);
  }

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnSessionRestoreStateChanged(user_profile_, state_));
}

void OAuth2LoginManager::SetSessionRestoreStartForTesting(
    const base::Time& time) {
  session_restore_start_ = time;
}

}  // namespace chromeos
