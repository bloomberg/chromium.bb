// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

// Values used from |MutableProfileOAuth2TokenServiceDelegate|.
const net::BackoffEntry::Policy kBackoffPolicy = {
    0 /* int num_errors_to_ignore */,

    1000 /* int initial_delay_ms */,

    2.0 /* double multiply_factor */,

    0.2 /* double jitter_factor */,

    15 * 60 * 1000 /* int64_t maximum_backoff_ms */,

    -1 /* int64_t entry_lifetime_ms */,

    false /* bool always_use_initial_delay */,
};

// Maps crOS Account Manager |account_keys| to the account id representation
// used by the OAuth token service chain. |account_keys| can safely contain Gaia
// and non-Gaia accounts. Non-Gaia accounts will be filtered out.
// |account_keys| is the set of accounts that need to be translated.
// |account_tracker_service| is an unowned pointer.
std::vector<std::string> GetOAuthAccountIdsFromAccountKeys(
    const std::set<AccountManager::AccountKey>& account_keys,
    const AccountTrackerService* const account_tracker_service) {
  std::vector<std::string> accounts;
  for (auto& account_key : account_keys) {
    if (account_key.account_type !=
        account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
      continue;
    }

    std::string account_id =
        account_tracker_service
            ->FindAccountInfoByGaiaId(account_key.id /* gaia_id */)
            .account_id;
    DCHECK(!account_id.empty());
    accounts.emplace_back(account_id);
  }

  return accounts;
}

}  // namespace

ChromeOSOAuth2TokenServiceDelegate::ChromeOSOAuth2TokenServiceDelegate(
    AccountTrackerService* account_tracker_service,
    chromeos::AccountManager* account_manager)
    : account_tracker_service_(account_tracker_service),
      account_manager_(account_manager),
      backoff_entry_(&kBackoffPolicy),
      backoff_error_(GoogleServiceAuthError::NONE),
      weak_factory_(this) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

ChromeOSOAuth2TokenServiceDelegate::~ChromeOSOAuth2TokenServiceDelegate() {
  account_manager_->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

OAuth2AccessTokenFetcher*
ChromeOSOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());

  ValidateAccountId(account_id);

  // Check if we need to reject the request.
  // We will reject the request if we are facing a persistent error for this
  // account.
  auto it = errors_.find(account_id);
  if (it != errors_.end() && it->second.last_auth_error.IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << it->second.last_auth_error.state();
    // |OAuth2TokenService| will manage the lifetime of this pointer.
    return new OAuth2AccessTokenFetcherImmediateError(
        consumer, it->second.last_auth_error);
  }
  // Or when we need to backoff.
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    // |OAuth2TokenService| will manage the lifetime of this pointer.
    return new OAuth2AccessTokenFetcherImmediateError(consumer, backoff_error_);
  }

  // |OAuth2TokenService| will manage the lifetime of the released pointer.
  return account_manager_
      ->CreateAccessTokenFetcher(
          AccountManager::AccountKey{
              account_tracker_service_->GetAccountInfo(account_id).gaia,
              account_manager::AccountType::
                  ACCOUNT_TYPE_GAIA} /* account_key */,
          url_loader_factory, consumer)
      .release();
}

// Note: This method should use the same logic for filtering accounts as
// |GetAccounts|. See crbug.com/919793 for details. At the time of writing,
// both |GetAccounts| and |RefreshTokenIsAvailable| use
// |GetOAuthAccountIdsFromAccountKeys|.
bool ChromeOSOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  if (load_credentials_state() != LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    return false;
  }

  // We intentionally do NOT check if the refresh token associated with
  // |account_id| is valid or not. See crbug.com/919793 for details.
  return base::ContainsValue(GetOAuthAccountIdsFromAccountKeys(
                                 account_keys_, account_tracker_service_),
                             account_id);
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    // Update the existing error.
    if (error.state() == GoogleServiceAuthError::NONE)
      errors_.erase(it);
    else
      it->second.last_auth_error = error;
    FireAuthErrorChanged(account_id, error);
  } else if (error.state() != GoogleServiceAuthError::NONE) {
    // Add a new error.
    errors_.emplace(account_id, AccountErrorStatus{error});
    FireAuthErrorChanged(account_id, error);
  }
}

GoogleServiceAuthError ChromeOSOAuth2TokenServiceDelegate::GetAuthError(
    const std::string& account_id) const {
  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    return it->second.last_auth_error;
  }

  return GoogleServiceAuthError::AuthErrorNone();
}

// Note: This method should use the same logic for filtering accounts as
// |RefreshTokenIsAvailable|. See crbug.com/919793 for details. At the time of
// writing, both |GetAccounts| and |RefreshTokenIsAvailable| use
// |GetOAuthAccountIdsFromAccountKeys|.
std::vector<std::string> ChromeOSOAuth2TokenServiceDelegate::GetAccounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |GetAccounts| intentionally does not care about the state of
  // |load_credentials_state|. See crbug.com/919793 and crbug.com/900590 for
  // details.

  return GetOAuthAccountIdsFromAccountKeys(account_keys_,
                                           account_tracker_service_);
}

void ChromeOSOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_credentials_state() != LOAD_CREDENTIALS_NOT_STARTED) {
    return;
  }

  set_load_credentials_state(LOAD_CREDENTIALS_IN_PROGRESS);

  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
  account_manager_->GetAccounts(
      base::BindOnce(&ChromeOSOAuth2TokenServiceDelegate::OnGetAccounts,
                     weak_factory_.GetWeakPtr()));
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  // This API could have been called for upserting the Device/Primary
  // |account_id| or a Secondary |account_id|.

  // Account insertion:
  // Device Account insertion on Chrome OS happens as a 2 step process:
  // 1. The account is inserted into SigninManager / AccountTrackerService, via
  // IdentityManager, with a valid Gaia id and email but an invalid refresh
  // token.
  // 2. This API is called to update the aforementioned account with a valid
  // refresh token.
  // Secondary Account insertion on Chrome OS happens atomically in
  // |InlineLoginHandlerChromeOS::<anon>::SigninHelper::OnClientOAuthSuccess|.
  // In both of the aforementioned cases, we can be sure that when this API is
  // called, |account_id| is guaranteed to be present in
  // |AccountTrackerService|. This guarantee is important because
  // |ChromeOSOAuth2TokenServiceDelegate| relies on |AccountTrackerService| to
  // convert |account_id| to an email id.

  // Account update:
  // If an account is being updated, it must be present in
  // |AccountTrackerService|.

  // Hence for all cases (insertion and updates for Device and Secondary
  // Accounts) we can be sure that |account_id| is present in
  // |AccountTrackerService|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);

  const AccountInfo& account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  LOG_IF(FATAL, account_info.gaia.empty())
      << "account_id must be present in AccountTrackerService before "
         "UpdateCredentials is called";

  // Will result in AccountManager calling
  // |ChromeOSOAuth2TokenServiceDelegate::OnTokenUpserted|.
  account_manager_->UpsertAccount(
      AccountManager::AccountKey{
          account_info.gaia,
          account_manager::AccountType::ACCOUNT_TYPE_GAIA} /* account_key */,
      account_info.email /* email */, refresh_token);
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeOSOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return account_manager_->GetUrlLoaderFactory();
}

void ChromeOSOAuth2TokenServiceDelegate::OnGetAccounts(
    const std::vector<AccountManager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This callback should only be triggered during |LoadCredentials|, which
  // implies that |load_credentials_state())| should in
  // |LOAD_CREDENTIALS_IN_PROGRESS| state.
  DCHECK_EQ(LOAD_CREDENTIALS_IN_PROGRESS, load_credentials_state());

  set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  // The typical order of |OAuth2TokenService::Observer| callbacks is:
  // 1. OnRefreshTokenAvailable
  // 2. OnEndBatchChanges
  // 3. OnRefreshTokensLoaded
  {
    ScopedBatchChange batch(this);
    for (const auto& account : accounts) {
      OnTokenUpserted(account);
    }
  }
  FireRefreshTokensLoaded();
}

void ChromeOSOAuth2TokenServiceDelegate::OnTokenUpserted(
    const AccountManager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  account_keys_.insert(account.key);

  if (account.key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return;
  }

  std::string email = account.raw_email;
  if (email.empty()) {
    // This is an old, un-migrated account for which we do not have an email id
    // stored on disk. Check https://crbug.com/933307 and
    // https://crbug.com/925827 for context.

    // This is an existing account and thus, must be present in
    // AccountTrackerService.
    email = account_tracker_service_
                ->FindAccountInfoByGaiaId(account.key.id /* gaia_id */)
                .email;

    // Additionally, backfill this email into Account Manager so that we can
    // remove this block of code with a DCHECK in future (M75) releases.
    account_manager_->UpdateEmail(account.key, email);
  }

  std::string account_id = account_tracker_service_->SeedAccountInfo(
      account.key.id /* gaia_id */, email);
  DCHECK(!account_id.empty());

  // Clear any previously cached errors for |account_id|.
  // We cannot directly use |UpdateAuthError| because it does not invoke
  // |FireAuthErrorChanged| if |account_id|'s error state was already
  // |GoogleServiceAuthError::State::NONE|, but |FireAuthErrorChanged| must be
  // invoked here, regardless. See the comment above |FireAuthErrorChanged| few
  // lines down.
  errors_.erase(account_id);
  GoogleServiceAuthError error(GoogleServiceAuthError::AuthErrorNone());

  // However, if we know that |account_key| has a dummy token, store a
  // persistent error against it, so that we can pre-emptively reject access
  // token requests for it.
  if (account_manager_->HasDummyGaiaToken(account.key)) {
    error = GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
            CREDENTIALS_REJECTED_BY_CLIENT);
    errors_.emplace(account_id, AccountErrorStatus{error});
  }

  ScopedBatchChange batch(this);
  FireRefreshTokenAvailable(account_id);
  // See |OAuth2TokenService::Observer::OnAuthErrorChanged|.
  // |OnAuthErrorChanged| must be always called after
  // |OnRefreshTokenAvailable|, when refresh token is updated.
  FireAuthErrorChanged(account_id, error);
}

void ChromeOSOAuth2TokenServiceDelegate::OnAccountRemoved(
    const AccountManager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());

  auto it = account_keys_.find(account.key);
  if (it == account_keys_.end()) {
    return;
  }
  account_keys_.erase(it);

  if (account.key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return;
  }
  std::string account_id =
      account_tracker_service_
          ->FindAccountInfoByGaiaId(account.key.id /* gaia_id */)
          .account_id;
  DCHECK(!account_id.empty());

  ScopedBatchChange batch(this);

  // ProfileOAuth2TokenService will clear its cache for |account_id| when this
  // is called. See |ProfileOAuth2TokenService::OnRefreshTokenRevoked|.
  FireRefreshTokenRevoked(account_id);
}

void ChromeOSOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

void ChromeOSOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

const net::BackoffEntry* ChromeOSOAuth2TokenServiceDelegate::BackoffEntry()
    const {
  return &backoff_entry_;
}

void ChromeOSOAuth2TokenServiceDelegate::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  backoff_entry_.Reset();
}

}  // namespace chromeos
