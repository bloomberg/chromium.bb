// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

OAuth2LoginManager::OAuth2LoginManager(Profile* user_profile)
    : user_profile_(user_profile),
      restore_strategy_(RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN),
      state_(SESSION_RESTORE_NOT_STARTED) {
  GetTokenService()->AddObserver(this);

  // For telemetry, we mark session restore completed to avoid warnings from
  // MergeSessionThrottle.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    SetSessionRestoreState(SESSION_RESTORE_DONE);
  }
}

OAuth2LoginManager::~OAuth2LoginManager() {}

void OAuth2LoginManager::AddObserver(OAuth2LoginManager::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OAuth2LoginManager::RemoveObserver(
    OAuth2LoginManager::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OAuth2LoginManager::RestoreSession(
    scoped_refptr<network::SharedURLLoaderFactory> auth_url_loader_factory,
    SessionRestoreStrategy restore_strategy,
    const std::string& oauth2_refresh_token,
    const std::string& oauth2_access_token) {
  DCHECK(user_profile_);
  auth_url_loader_factory_ = auth_url_loader_factory;
  restore_strategy_ = restore_strategy;
  refresh_token_ = oauth2_refresh_token;
  oauthlogin_access_token_ = oauth2_access_token;
  session_restore_start_ = base::Time::Now();
  ContinueSessionRestore();
}

void OAuth2LoginManager::ContinueSessionRestore() {
  DCHECK_NE(DEPRECATED_RESTORE_FROM_COOKIE_JAR, restore_strategy_)
      << "Exchanging cookies for OAuth 2.0 tokens is no longer supported";

  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_PREPARING);

  // Save passed OAuth2 refresh token.
  if (restore_strategy_ == RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN) {
    DCHECK(!refresh_token_.empty());
    restore_strategy_ = RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
    StoreOAuth2Token();
    return;
  }

  DCHECK_EQ(RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN, restore_strategy_);
  RestoreSessionFromSavedTokens();
}

void OAuth2LoginManager::RestoreSessionFromSavedTokens() {
  // Just return if there is a pending TokenService::LoadCredentials call.
  // Session restore continues in OnRefreshTokenAvailable when the call
  // finishes.
  if (pending_token_service_load_)
    return;

  ProfileOAuth2TokenService* token_service = GetTokenService();
  const std::string primary_account_id = GetPrimaryAccountId();
  if (token_service->RefreshTokenIsAvailable(primary_account_id)) {
    VLOG(1) << "OAuth2 refresh token is already loaded.";
    FireRefreshTokensLoaded();
    VerifySessionCookies();
  } else {
    VLOG(1) << "Loading OAuth2 refresh token from database.";

    // Flag user with unknown token status in case there are no saved tokens
    // and OnRefreshTokenAvailable is not called. Flagging it here would
    // cause user to go through Gaia in next login to obtain a new refresh
    // token.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        AccountId::FromUserEmail(primary_account_id),
        user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN);

    pending_token_service_load_ = true;
    token_service->LoadCredentials(primary_account_id);
  }
}

void OAuth2LoginManager::Stop() {
  oauth2_token_fetcher_.reset();
  login_verifier_.reset();
}

bool OAuth2LoginManager::SessionRestoreIsRunning() const {
  return state_ == SESSION_RESTORE_PREPARING ||
         state_ == SESSION_RESTORE_IN_PROGRESS;
}

bool OAuth2LoginManager::ShouldBlockTabLoading() const {
  return SessionRestoreIsRunning();
}

void OAuth2LoginManager::OnRefreshTokenAvailable(
    const std::string& user_email) {
  VLOG(1) << "OnRefreshTokenAvailable";

  if (state_ == SESSION_RESTORE_NOT_STARTED)
    return;

  // TODO(fgorski): Once ProfileOAuth2TokenService supports multi-login, make
  // sure to restore session cookies in the context of the correct user_email.

  // Do not validate tokens for supervised users, as they don't actually have
  // oauth2 token.
  if (user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    VLOG(1) << "Logged in as supervised user, skip token validation.";
    return;
  }
  // Only restore session cookies for the primary account in the profile.
  if (GetPrimaryAccountId() == user_email) {
    // The refresh token has changed, so stop any ongoing actions that were
    // based on the old refresh token.
    Stop();

    // Token is loaded. Undo the flagging before token loading.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        AccountId::FromUserEmail(user_email),
        user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    pending_token_service_load_ = false;
    VerifySessionCookies();
  }
}

ProfileOAuth2TokenService* OAuth2LoginManager::GetTokenService() {
  return ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_);
}

std::string OAuth2LoginManager::GetPrimaryAccountId() {
  const std::string primary_account_id =
      IdentityManagerFactory::GetForProfile(user_profile_)
          ->GetPrimaryAccountId();
  LOG_IF(ERROR, primary_account_id.empty()) << "Primary account id is empty.";
  return primary_account_id;
}

void OAuth2LoginManager::StoreOAuth2Token() {
  UpdateCredentials(GetPrimaryAccountId());
}

void OAuth2LoginManager::UpdateCredentials(const std::string& account_id) {
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token_.empty());
  // |account_id| is assumed to be already canonicalized if it's an email.
  GetTokenService()->UpdateCredentials(account_id, refresh_token_);
  FireRefreshTokensLoaded();

  for (auto& observer : observer_list_)
    observer.OnNewRefreshTokenAvaiable(user_profile_);
}

void OAuth2LoginManager::FireRefreshTokensLoaded() {
  // TODO(570218): Figure out the right way to plumb this.
  GetTokenService()->LoadCredentials(std::string());
}

void OAuth2LoginManager::OnOAuth2TokensAvailable(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  VLOG(1) << "OAuth2 tokens fetched";
  DCHECK(refresh_token_.empty());
  refresh_token_.assign(oauth2_tokens.refresh_token);
  oauthlogin_access_token_ = oauth2_tokens.access_token;
  StoreOAuth2Token();
}

void OAuth2LoginManager::OnOAuth2TokensFetchFailed() {
  LOG(ERROR) << "OAuth2 tokens fetch failed!";
  RecordSessionRestoreOutcome(SESSION_RESTORE_TOKEN_FETCH_FAILED,
                              SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::VerifySessionCookies() {
  DCHECK(!login_verifier_.get());
  login_verifier_.reset(new OAuth2LoginVerifier(
      this, GaiaCookieManagerServiceFactory::GetForProfile(user_profile_),
      GetPrimaryAccountId(), oauthlogin_access_token_));

  if (restore_strategy_ == RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN) {
    login_verifier_->VerifyUserCookies();
    return;
  }

  RestoreSessionCookies();
}

void OAuth2LoginManager::RestoreSessionCookies() {
  SetSessionRestoreState(SESSION_RESTORE_IN_PROGRESS);
  login_verifier_->VerifyProfileTokens();
}

void OAuth2LoginManager::Shutdown() {
  GetTokenService()->RemoveObserver(this);
  login_verifier_.reset();
  oauth2_token_fetcher_.reset();
}

void OAuth2LoginManager::OnSessionMergeSuccess() {
  VLOG(1) << "OAuth2 refresh and/or GAIA token verification succeeded.";
  RecordSessionRestoreOutcome(SESSION_RESTORE_SUCCESS, SESSION_RESTORE_DONE);
}

void OAuth2LoginManager::OnSessionMergeFailure(bool connection_error) {
  LOG(ERROR) << "OAuth2 refresh and GAIA token verification failed!"
             << " connection_error: " << connection_error;
  RecordSessionRestoreOutcome(SESSION_RESTORE_MERGE_SESSION_FAILED,
                              connection_error
                                  ? SESSION_RESTORE_CONNECTION_FAILED
                                  : SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::OnListAccountsSuccess(
    const std::vector<gaia::ListedAccount>& accounts) {
  MergeVerificationOutcome outcome = POST_MERGE_SUCCESS;
  // Let's analyze which accounts we see logged in here:
  std::string user_email = gaia::CanonicalizeEmail(GetPrimaryAccountId());
  if (!accounts.empty()) {
    bool found = false;
    bool first = true;
    for (std::vector<gaia::ListedAccount>::const_iterator iter =
             accounts.begin();
         iter != accounts.end(); ++iter) {
      if (iter->email == user_email) {
        found = iter->valid;
        break;
      }

      first = false;
    }

    if (!found)
      outcome = POST_MERGE_MISSING_PRIMARY_ACCOUNT;
    else if (!first)
      outcome = POST_MERGE_PRIMARY_NOT_FIRST_ACCOUNT;

  } else {
    outcome = POST_MERGE_NO_ACCOUNTS;
  }

  bool is_pre_merge = (state_ == SESSION_RESTORE_PREPARING);
  RecordCookiesCheckOutcome(is_pre_merge, outcome);
  // If the primary account is missing during the initial cookie freshness
  // check, try to restore GAIA session cookies form the OAuth2 tokens.
  if (is_pre_merge) {
    if (outcome != POST_MERGE_SUCCESS &&
        outcome != POST_MERGE_PRIMARY_NOT_FIRST_ACCOUNT) {
      RestoreSessionCookies();
    } else {
      // We are done with this account, it's GAIA cookies are legit.
      RecordSessionRestoreOutcome(SESSION_RESTORE_NOT_NEEDED,
                                  SESSION_RESTORE_DONE);
    }
  }
}

void OAuth2LoginManager::OnListAccountsFailure(bool connection_error) {
  bool is_pre_merge = (state_ == SESSION_RESTORE_PREPARING);
  RecordCookiesCheckOutcome(is_pre_merge, connection_error
                                              ? POST_MERGE_CONNECTION_FAILED
                                              : POST_MERGE_VERIFICATION_FAILED);
  if (is_pre_merge) {
    if (!connection_error) {
      // If we failed to get account list, our cookies might be stale so we
      // need to attempt to restore them.
      RestoreSessionCookies();
    } else {
      RecordSessionRestoreOutcome(SESSION_RESTORE_LISTACCOUNTS_FAILED,
                                  SESSION_RESTORE_CONNECTION_FAILED);
    }
  }
}

void OAuth2LoginManager::RecordSessionRestoreOutcome(
    SessionRestoreOutcome outcome,
    OAuth2LoginManager::SessionRestoreState state) {
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore", outcome,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(state);
}

// static
void OAuth2LoginManager::RecordCookiesCheckOutcome(
    bool is_pre_merge,
    MergeVerificationOutcome outcome) {
  if (is_pre_merge) {
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.PreMergeVerification", outcome,
                              POST_MERGE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.PostMergeVerification", outcome,
                              POST_MERGE_COUNT);
  }
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

  for (auto& observer : observer_list_)
    observer.OnSessionRestoreStateChanged(user_profile_, state_);
}

void OAuth2LoginManager::SetSessionRestoreStartForTesting(
    const base::Time& time) {
  session_restore_start_ = time;
}

}  // namespace chromeos
