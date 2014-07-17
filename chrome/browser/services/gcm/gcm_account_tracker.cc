// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace gcm {

namespace {
const char kGCMGroupServerScope[] = "https://www.googleapis.com/auth/gcm";
const char kGCMAccountTrackerName[] = "gcm_account_tracker";
}  // namespace

GCMAccountTracker::AccountInfo::AccountInfo(const std::string& email,
                                            AccountState state)
    : email(email), state(state) {
}

GCMAccountTracker::AccountInfo::~AccountInfo() {
}

GCMAccountTracker::GCMAccountTracker(
    scoped_ptr<gaia::AccountTracker> account_tracker,
    const UpdateAccountsCallback& callback)
    : OAuth2TokenService::Consumer(kGCMAccountTrackerName),
      account_tracker_(account_tracker.release()),
      callback_(callback),
      shutdown_called_(false) {
  DCHECK(!callback_.is_null());
}

GCMAccountTracker::~GCMAccountTracker() {
  DCHECK(shutdown_called_);
}

void GCMAccountTracker::Shutdown() {
  Stop();
  shutdown_called_ = true;
  account_tracker_->Shutdown();
}

void GCMAccountTracker::Start() {
  DCHECK(!shutdown_called_);
  account_tracker_->AddObserver(this);

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

void GCMAccountTracker::Stop() {
  DCHECK(!shutdown_called_);
  account_tracker_->RemoveObserver(this);
  pending_token_requests_.clear();
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

void GCMAccountTracker::CompleteCollectingTokens() {
  DCHECK(!callback_.is_null());
  // Wait for gaia::AccountTracker to be done with fetching the user info, as
  // well as all of the pending token requests from GCMAccountTracker to be done
  // before you report the results.
  if (!account_tracker_->IsAllUserInfoFetched() ||
      !pending_token_requests_.empty()) {
    return;
  }

  bool account_removed = false;
  std::map<std::string, std::string> account_tokens;
  for (AccountInfos::iterator iter = account_infos_.begin();
       iter != account_infos_.end();) {
    switch (iter->second.state) {
      case ACCOUNT_REMOVED:
        // We only mark accounts as removed when there was an account that was
        // explicitly signed out.
        account_removed = true;
        // We also stop tracking the account, now that it will be reported as
        // removed.
        account_infos_.erase(iter++);
        break;

      case TOKEN_PRESENT:
        account_tokens[iter->second.email] = iter->second.access_token;
        ++iter;
        break;

      case GETTING_TOKEN:
        // This should not happen, as we are making a check that there are no
        // pending requests above.
        NOTREACHED();
        ++iter;
        break;

      case TOKEN_NEEDED:
        // We failed to fetch an access token for the account, but it has not
        // been signed out (perhaps there is a network issue). We don't report
        // it, but next time there is a sign-in change we will update its state.
        ++iter;
        break;
    }
  }

  // Make sure that there is something to report, otherwise bail out.
  if (!account_tokens.empty() || account_removed) {
    DVLOG(1) << "Calling callback: " << account_tokens.size();
    callback_.Run(account_tokens);
  } else {
    DVLOG(1) << "No tokens and nothing removed. Skipping callback.";
  }
}

void GCMAccountTracker::DeleteTokenRequest(
    const OAuth2TokenService::Request* request) {
  ScopedVector<OAuth2TokenService::Request>::iterator iter = std::find(
      pending_token_requests_.begin(), pending_token_requests_.end(), request);
  if (iter != pending_token_requests_.end())
    pending_token_requests_.erase(iter);
}

void GCMAccountTracker::GetAllNeededTokens() {
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
