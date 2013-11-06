// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/account_tracker.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "content/public/browser/notification_details.h"

namespace extensions {

AccountTracker::AccountTracker(Profile* profile) : profile_(profile) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));

  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->AddObserver(this);
  SigninGlobalError::GetForProfile(profile_)->AddProvider(this);
}

AccountTracker::~AccountTracker() {}

void AccountTracker::ReportAuthError(const std::string& account_id,
                                     const GoogleServiceAuthError& error) {
  account_errors_.insert(make_pair(account_id, error));
  SigninGlobalError::GetForProfile(profile_)->AuthStatusChanged();
  UpdateSignInState(account_id, false);
}

void AccountTracker::Shutdown() {
  STLDeleteValues(&user_info_requests_);
  SigninGlobalError::GetForProfile(profile_)->RemoveProvider(this);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      RemoveObserver(this);
}

void AccountTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountTracker::OnRefreshTokenAvailable(const std::string& account_id) {
  // Ignore refresh tokens if there is no primary account ID at all.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  if (token_service->GetPrimaryAccountId().empty())
    return;

  DVLOG(1) << "AVAILABLE " << account_id;
  ClearAuthError(account_id);
  UpdateSignInState(account_id, true);
}

void AccountTracker::OnRefreshTokenRevoked(const std::string& account_id) {
  DVLOG(1) << "REVOKED " << account_id;
  UpdateSignInState(account_id, false);
}

void AccountTracker::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      StopTrackingAccount(content::Details<GoogleServiceSignoutDetails>(
          details)->username);
      break;
    default:
      NOTREACHED();
  }
}

void AccountTracker::NotifyAccountAdded(const AccountState& account) {
  DCHECK(!account.ids.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountAdded(account.ids));
}

void AccountTracker::NotifyAccountRemoved(const AccountState& account) {
  DCHECK(!account.ids.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountRemoved(account.ids));
}

void AccountTracker::NotifySignInChanged(const AccountState& account) {
  DCHECK(!account.ids.gaia.empty());
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnAccountSignInChanged(account.ids, account.is_signed_in));
}

void AccountTracker::ClearAuthError(const std::string& account_key) {
  account_errors_.erase(account_key);
  SigninGlobalError::GetForProfile(profile_)->AuthStatusChanged();
}

void AccountTracker::UpdateSignInState(const std::string& account_key,
                                       bool is_signed_in) {
  StartTrackingAccount(account_key);
  AccountState& account = accounts_[account_key];
  bool needs_gaia_id = account.ids.gaia.empty();
  bool was_signed_in = account.is_signed_in;
  account.is_signed_in = is_signed_in;

  if (needs_gaia_id && is_signed_in)
    StartFetchingUserInfo(account_key);

  if (!needs_gaia_id && (was_signed_in != is_signed_in))
    NotifySignInChanged(account);
}

void AccountTracker::StartTrackingAccount(const std::string& account_key) {
  if (!ContainsKey(accounts_, account_key)) {
    DVLOG(1) << "StartTracking " << account_key;
    AccountState account_state;
    account_state.ids.account_key = account_key;
    account_state.ids.email = account_key;
    account_state.is_signed_in = false;
    accounts_.insert(make_pair(account_key, account_state));
  }
}

void AccountTracker::StopTrackingAccount(const std::string& account_key) {
  if (ContainsKey(accounts_, account_key)) {
    AccountState& account = accounts_[account_key];
    if (!account.ids.gaia.empty()) {
      UpdateSignInState(account_key, false);
      NotifyAccountRemoved(account);
    }
    accounts_.erase(account_key);
  }

  ClearAuthError(account_key);

  if (ContainsKey(user_info_requests_, account_key))
    DeleteFetcher(user_info_requests_[account_key]);
}

void AccountTracker::StartFetchingUserInfo(const std::string& account_key) {
  if (ContainsKey(user_info_requests_, account_key))
    DeleteFetcher(user_info_requests_[account_key]);

  DVLOG(1) << "StartFetching " << account_key;
  AccountIdFetcher* fetcher =
      new AccountIdFetcher(profile_, this, account_key);
  user_info_requests_[account_key] = fetcher;
  fetcher->Start();
}

void AccountTracker::OnUserInfoFetchSuccess(AccountIdFetcher* fetcher,
                                            const std::string& gaia_id) {
  const std::string& account_key = fetcher->account_key();
  DCHECK(ContainsKey(accounts_, account_key));
  AccountState& account = accounts_[account_key];

  account.ids.gaia = gaia_id;
  NotifyAccountAdded(account);

  if (account.is_signed_in)
    NotifySignInChanged(account);

  DeleteFetcher(fetcher);
}

void AccountTracker::OnUserInfoFetchFailure(AccountIdFetcher* fetcher) {
  LOG(WARNING) << "Failed to get UserInfo for " << fetcher->account_key();
  std::string key = fetcher->account_key();
  DeleteFetcher(fetcher);
  StopTrackingAccount(key);
}

std::string AccountTracker::GetAccountId() const {
  if (account_errors_.size() == 0)
    return std::string();
  else
    return account_errors_.begin()->first;
}

GoogleServiceAuthError AccountTracker::GetAuthStatus() const {
  if (account_errors_.size() == 0)
    return GoogleServiceAuthError::AuthErrorNone();
  else
    return account_errors_.begin()->second;
}

void AccountTracker::DeleteFetcher(AccountIdFetcher* fetcher) {
  const std::string& account_key = fetcher->account_key();
  DCHECK(ContainsKey(user_info_requests_, account_key));
  DCHECK_EQ(fetcher, user_info_requests_[account_key]);
  user_info_requests_.erase(account_key);
  delete fetcher;
}

AccountIdFetcher::AccountIdFetcher(Profile* profile,
                                   AccountTracker* tracker,
                                   const std::string& account_key)
    : profile_(profile),
      tracker_(tracker),
      account_key_(account_key) {}

AccountIdFetcher::~AccountIdFetcher() {}

void AccountIdFetcher::Start() {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  login_token_request_ = service->StartRequest(
      account_key_, OAuth2TokenService::ScopeSet(), this);
}

void AccountIdFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(
      g_browser_process->system_request_context()));

  const int kMaxGetUserIdRetries = 3;
  gaia_oauth_client_->GetUserId(access_token, kMaxGetUserIdRetries, this);
}

void AccountIdFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OnGetTokenFailure: " << error.error_message();
  DCHECK_EQ(request, login_token_request_.get());
  tracker_->OnUserInfoFetchFailure(this);
}

void AccountIdFetcher::OnGetUserIdResponse(const std::string& gaia_id) {
  tracker_->OnUserInfoFetchSuccess(this, gaia_id);
}

void AccountIdFetcher::OnOAuthError() {
  LOG(ERROR) << "OnOAuthError";
  tracker_->OnUserInfoFetchFailure(this);
}

void AccountIdFetcher::OnNetworkError(int response_code) {
  LOG(ERROR) << "OnNetworkError " << response_code;
  tracker_->OnUserInfoFetchFailure(this);
}

}  // namespace extensions
