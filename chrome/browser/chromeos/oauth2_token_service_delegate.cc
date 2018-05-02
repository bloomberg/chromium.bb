// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"

namespace chromeos {

ChromeOSOAuth2TokenServiceDelegate::ChromeOSOAuth2TokenServiceDelegate(
    AccountTrackerService* account_tracker_service,
    chromeos::AccountManager* account_manager)
    : account_tracker_service_(account_tracker_service),
      account_manager_(account_manager),
      weak_factory_(this) {
  DCHECK(account_manager_);
  load_credentials_state_ = LOAD_CREDENTIALS_IN_PROGRESS;
  account_manager_->AddObserver(this);
  account_manager_->GetAccounts(
      base::BindOnce(&ChromeOSOAuth2TokenServiceDelegate::GetAccountsCallback,
                     weak_factory_.GetWeakPtr()));
}

ChromeOSOAuth2TokenServiceDelegate::~ChromeOSOAuth2TokenServiceDelegate() {
  account_manager_->RemoveObserver(this);
}

OAuth2AccessTokenFetcher*
ChromeOSOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state_);

  ValidateAccountId(account_id);

  const AccountManager::AccountKey& account_key =
      MapAccountIdToAccountKey(account_id);

  // |OAuth2TokenService| will manage the lifetime of the released pointer.
  return account_manager_
      ->CreateAccessTokenFetcher(account_key, getter, consumer)
      .release();
}

bool ChromeOSOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  if (load_credentials_state_ != LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    return false;
  }

  return account_manager_->IsTokenAvailable(
      MapAccountIdToAccountKey(account_id));
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sinhak): Implement a backoff policy.
  if (error.IsTransientError()) {
    return;
  }

  auto it = errors_.find(account_id);
  if (error.state() == GoogleServiceAuthError::NONE) {
    if (it != errors_.end()) {
      errors_.erase(it);
      FireAuthErrorChanged(account_id, error);
    }
  } else if ((it == errors_.end()) || (it->second != error)) {
    errors_[account_id] = error;
    FireAuthErrorChanged(account_id, error);
  }
}

GoogleServiceAuthError ChromeOSOAuth2TokenServiceDelegate::GetAuthError(
    const std::string& account_id) const {
  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    return it->second;
  }

  return GoogleServiceAuthError::AuthErrorNone();
}

std::vector<std::string> ChromeOSOAuth2TokenServiceDelegate::GetAccounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state_);

  std::vector<std::string> accounts;
  for (auto& account_key : account_keys_) {
    std::string account_id = MapAccountKeyToAccountId(account_key);
    if (!account_id.empty()) {
      accounts.emplace_back(account_id);
    }
  }

  return accounts;
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);

  const AccountManager::AccountKey& account_key =
      MapAccountIdToAccountKey(account_id);
  account_keys_.insert(account_key);
  account_manager_->UpsertToken(account_key, refresh_token);

  FireRefreshTokenAvailable(account_id);
}

net::URLRequestContextGetter*
ChromeOSOAuth2TokenServiceDelegate::GetRequestContext() const {
  // LSTs on Chrome are not channel/token bound for now and hence we can use
  // the system request context.
  // Note that we cannot use the Profile's request context since
  // |AccountManager| acts outside the scope of Profiles.
  // TODO(sinhak): Create a new |URLRequestContext| for |AccountManager| which
  // conforms to token binding when those details are finalized.
  return g_browser_process->system_request_context();
}

OAuth2TokenServiceDelegate::LoadCredentialsState
ChromeOSOAuth2TokenServiceDelegate::GetLoadCredentialsState() const {
  return load_credentials_state_;
}

void ChromeOSOAuth2TokenServiceDelegate::GetAccountsCallback(
    std::vector<AccountManager::AccountKey> account_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnAccountListUpdated(account_keys);

  load_credentials_state_ = LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS;
  FireRefreshTokensLoaded();
  for (const auto& account_key : account_keys) {
    std::string account_id = MapAccountKeyToAccountId(account_key);
    if (!account_id.empty()) {
      FireRefreshTokenAvailable(account_id);
    }
  }
}

std::string ChromeOSOAuth2TokenServiceDelegate::MapAccountKeyToAccountId(
    const AccountManager::AccountKey& account_key) const {
  DCHECK(account_key.IsValid());

  if (account_key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return std::string();
  }

  const std::string& account_id =
      account_tracker_service_->FindAccountInfoByGaiaId(account_key.id)
          .account_id;
  DCHECK(!account_id.empty()) << "Can't find account id";
  return account_id;
}

AccountManager::AccountKey
ChromeOSOAuth2TokenServiceDelegate::MapAccountIdToAccountKey(
    const std::string& account_id) const {
  DCHECK(!account_id.empty());

  const AccountInfo& account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  DCHECK(!account_info.gaia.empty()) << "Can't find account info";
  return AccountManager::AccountKey{
      account_info.gaia, account_manager::AccountType::ACCOUNT_TYPE_GAIA};
}

void ChromeOSOAuth2TokenServiceDelegate::OnAccountListUpdated(
    const std::vector<AccountManager::AccountKey>& account_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  account_keys_.clear();
  account_keys_.insert(account_keys.begin(), account_keys.end());
}

}  // namespace chromeos
