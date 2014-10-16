// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/time/time.h"
#include "components/gcm_driver/gcm_driver.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/ip_endpoint.h"

namespace gcm {

namespace {
const char kGCMGroupServerScope[] = "https://www.googleapis.com/auth/gcm";
const char kGCMCheckinServerScope[] =
    "https://www.googleapis.com/auth/android_checkin";
const char kGCMAccountTrackerName[] = "gcm_account_tracker";
const int64 kMinimumTokenValidityMs = 500;
}  // namespace

GCMAccountTracker::AccountInfo::AccountInfo(const std::string& email,
                                            AccountState state)
    : email(email), state(state) {
}

GCMAccountTracker::AccountInfo::~AccountInfo() {
}

GCMAccountTracker::GCMAccountTracker(
    scoped_ptr<gaia::AccountTracker> account_tracker,
    GCMDriver* driver)
    : OAuth2TokenService::Consumer(kGCMAccountTrackerName),
      account_tracker_(account_tracker.release()),
      driver_(driver),
      shutdown_called_(false) {
}

GCMAccountTracker::~GCMAccountTracker() {
  DCHECK(shutdown_called_);
}

void GCMAccountTracker::Shutdown() {
  shutdown_called_ = true;
  driver_->RemoveConnectionObserver(this);
  account_tracker_->RemoveObserver(this);
  account_tracker_->Shutdown();
}

void GCMAccountTracker::Start() {
  DCHECK(!shutdown_called_);
  account_tracker_->AddObserver(this);
  driver_->AddConnectionObserver(this);

  std::vector<gaia::AccountIds> accounts = account_tracker_->GetAccounts();
  if (accounts.empty()) {
    CompleteCollectingTokens();
    return;
  }

  for (std::vector<gaia::AccountIds>::const_iterator iter = accounts.begin();
       iter != accounts.end();
       ++iter) {
    if (!iter->email.empty()) {
      account_infos_.insert(std::make_pair(
          iter->account_key, AccountInfo(iter->email, TOKEN_NEEDED)));
    }
  }

  GetAllNeededTokens();
}

void GCMAccountTracker::OnAccountAdded(const gaia::AccountIds& ids) {
  DVLOG(1) << "Account added: " << ids.email;
  // We listen for the account signing in, which happens after account is added.
}

void GCMAccountTracker::OnAccountRemoved(const gaia::AccountIds& ids) {
  DVLOG(1) << "Account removed: " << ids.email;
  // We listen for the account signing out, which happens before account is
  // removed.
}

void GCMAccountTracker::OnAccountSignInChanged(const gaia::AccountIds& ids,
                                               bool is_signed_in) {
  if (is_signed_in)
    OnAccountSignedIn(ids);
  else
    OnAccountSignedOut(ids);
}

void GCMAccountTracker::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(request);
  DCHECK(!request->GetAccountId().empty());
  DVLOG(1) << "Get token success: " << request->GetAccountId();

  AccountInfos::iterator iter = account_infos_.find(request->GetAccountId());
  DCHECK(iter != account_infos_.end());
  if (iter != account_infos_.end()) {
    DCHECK(iter->second.state == GETTING_TOKEN ||
           iter->second.state == ACCOUNT_REMOVED);
    // If OnAccountSignedOut(..) was called most recently, account is kept in
    // ACCOUNT_REMOVED state.
    if (iter->second.state == GETTING_TOKEN) {
      iter->second.state = TOKEN_PRESENT;
      iter->second.access_token = access_token;
      iter->second.expiration_time = expiration_time;
    }
  }

  DeleteTokenRequest(request);
  CompleteCollectingTokens();
}

void GCMAccountTracker::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(request);
  DCHECK(!request->GetAccountId().empty());
  DVLOG(1) << "Get token failure: " << request->GetAccountId();

  AccountInfos::iterator iter = account_infos_.find(request->GetAccountId());
  DCHECK(iter != account_infos_.end());
  if (iter != account_infos_.end()) {
    DCHECK(iter->second.state == GETTING_TOKEN ||
           iter->second.state == ACCOUNT_REMOVED);
    // If OnAccountSignedOut(..) was called most recently, account is kept in
    // ACCOUNT_REMOVED state.
    if (iter->second.state == GETTING_TOKEN)
      iter->second.state = TOKEN_NEEDED;
  }

  DeleteTokenRequest(request);
  CompleteCollectingTokens();
}

void GCMAccountTracker::OnConnected(const net::IPEndPoint& ip_endpoint) {
  if (SanitizeTokens())
    GetAllNeededTokens();
}

void GCMAccountTracker::OnDisconnected() {
  // We are disconnected, so no point in trying to work with tokens.
}

void GCMAccountTracker::CompleteCollectingTokens() {
  // Make sure all tokens are valid.
  if (SanitizeTokens()) {
    GetAllNeededTokens();
    return;
  }

  // Wait for gaia::AccountTracker to be done with fetching the user info, as
  // well as all of the pending token requests from GCMAccountTracker to be done
  // before you report the results.
  if (!account_tracker_->IsAllUserInfoFetched() ||
      !pending_token_requests_.empty()) {
    return;
  }

  bool account_removed = false;
  // Stop tracking the accounts, that were removed, as it will be reported to
  // the driver.
  for (AccountInfos::iterator iter = account_infos_.begin();
       iter != account_infos_.end();) {
    if (iter->second.state == ACCOUNT_REMOVED) {
      account_removed = true;
      account_infos_.erase(iter++);
    } else {
      ++iter;
    }
  }

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  for (AccountInfos::iterator iter = account_infos_.begin();
       iter != account_infos_.end(); ++iter) {
    if (iter->second.state == TOKEN_PRESENT) {
      GCMClient::AccountTokenInfo token_info;
      token_info.account_id = iter->first;
      token_info.email = iter->second.email;
      token_info.access_token = iter->second.access_token;
      account_tokens.push_back(token_info);
    } else {
      // This should not happen, as we are making a check that there are no
      // pending requests above, stopping tracking of removed accounts, or start
      // fetching tokens.
      NOTREACHED();
    }
  }

  // Make sure that there is something to report, otherwise bail out.
  if (!account_tokens.empty() || account_removed) {
    DVLOG(1) << "Reporting the tokens to driver: " << account_tokens.size();
    driver_->SetAccountTokens(account_tokens);
  } else {
    DVLOG(1) << "No tokens and nothing removed. Skipping callback.";
  }
}

bool GCMAccountTracker::SanitizeTokens() {
  bool tokens_needed = false;
  for (AccountInfos::iterator iter = account_infos_.begin();
       iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_PRESENT &&
        iter->second.expiration_time <
            base::Time::Now() +
                base::TimeDelta::FromMilliseconds(kMinimumTokenValidityMs)) {
      iter->second.access_token.clear();
      iter->second.state = TOKEN_NEEDED;
      iter->second.expiration_time = base::Time();
    }

    if (iter->second.state == TOKEN_NEEDED)
      tokens_needed = true;
  }

  return tokens_needed;
}

void GCMAccountTracker::DeleteTokenRequest(
    const OAuth2TokenService::Request* request) {
  ScopedVector<OAuth2TokenService::Request>::iterator iter = std::find(
      pending_token_requests_.begin(), pending_token_requests_.end(), request);
  if (iter != pending_token_requests_.end())
    pending_token_requests_.erase(iter);
}

void GCMAccountTracker::GetAllNeededTokens() {
  // Only start fetching tokens if driver is running, they have a limited
  // validity time and GCM connection is a good indication of network running.
  if (!driver_->IsConnected())
    return;

  for (AccountInfos::iterator iter = account_infos_.begin();
       iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_NEEDED)
      GetToken(iter);
  }
}

void GCMAccountTracker::GetToken(AccountInfos::iterator& account_iter) {
  DCHECK(GetTokenService());
  DCHECK_EQ(account_iter->second.state, TOKEN_NEEDED);

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kGCMGroupServerScope);
  scopes.insert(kGCMCheckinServerScope);
  scoped_ptr<OAuth2TokenService::Request> request =
      GetTokenService()->StartRequest(account_iter->first, scopes, this);

  pending_token_requests_.push_back(request.release());
  account_iter->second.state = GETTING_TOKEN;
}

void GCMAccountTracker::OnAccountSignedIn(const gaia::AccountIds& ids) {
  DVLOG(1) << "Account signed in: " << ids.email;
  AccountInfos::iterator iter = account_infos_.find(ids.account_key);
  if (iter == account_infos_.end()) {
    DCHECK(!ids.email.empty());
    account_infos_.insert(
        std::make_pair(ids.account_key, AccountInfo(ids.email, TOKEN_NEEDED)));
  } else if (iter->second.state == ACCOUNT_REMOVED) {
    iter->second.state = TOKEN_NEEDED;
  }

  GetAllNeededTokens();
}

void GCMAccountTracker::OnAccountSignedOut(const gaia::AccountIds& ids) {
  DVLOG(1) << "Account signed out: " << ids.email;
  AccountInfos::iterator iter = account_infos_.find(ids.account_key);
  if (iter == account_infos_.end())
    return;

  iter->second.access_token.clear();
  iter->second.state = ACCOUNT_REMOVED;
  CompleteCollectingTokens();
}

OAuth2TokenService* GCMAccountTracker::GetTokenService() {
  DCHECK(account_tracker_->identity_provider());
  return account_tracker_->identity_provider()->GetTokenService();
}

}  // namespace gcm
