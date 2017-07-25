// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/webdata/common/web_data_service_base.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kAccountIdPrefix[] = "AccountId-";
const size_t kAccountIdPrefixLength = 10;

std::string ApplyAccountIdPrefix(const std::string& account_id) {
  return kAccountIdPrefix + account_id;
}

bool IsLegacyRefreshTokenId(const std::string& service_id) {
  return service_id == GaiaConstants::kGaiaOAuth2LoginRefreshToken;
}

bool IsLegacyServiceId(const std::string& account_id) {
  return account_id.compare(0u, kAccountIdPrefixLength, kAccountIdPrefix) != 0;
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(kAccountIdPrefixLength);
}

OAuth2TokenServiceDelegate::LoadCredentialsState
LoadCredentialsStateFromTokenResult(TokenServiceTable::Result token_result) {
  switch (token_result) {
    case TokenServiceTable::TOKEN_DB_RESULT_SQL_INVALID_STATEMENT:
    case TokenServiceTable::TOKEN_DB_RESULT_BAD_ENTRY:
      return OAuth2TokenServiceDelegate::
          LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_DECRYPT_ERROR:
      return OAuth2TokenServiceDelegate::
          LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_SUCCESS:
      return OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS;
  }
  NOTREACHED();
  return OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_UNKNOWN;
}

}  // namespace

// This class sends a request to GAIA to revoke the given refresh token from
// the server.  This is a best effort attempt only.  This class deletes itself
// when done successfully or otherwise.
class MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken
    : public GaiaAuthConsumer {
 public:
  RevokeServerRefreshToken(
      MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
      const std::string& account_id);
  ~RevokeServerRefreshToken() override;

 private:
  // GaiaAuthConsumer overrides:
  void OnOAuth2RevokeTokenCompleted() override;

  MutableProfileOAuth2TokenServiceDelegate* token_service_delegate_;
  GaiaAuthFetcher fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RevokeServerRefreshToken);
};

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    RevokeServerRefreshToken(
        MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
        const std::string& refresh_token)
    : token_service_delegate_(token_service_delegate),
      fetcher_(this,
               GaiaConstants::kChromeSource,
               token_service_delegate_->GetRequestContext()) {
  fetcher_.StartRevokeOAuth2Token(refresh_token);
}

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    ~RevokeServerRefreshToken() {
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    OnOAuth2RevokeTokenCompleted() {
  // |this| pointer will be deleted when removed from the vector, so don't
  // access any members after call to erase().
  token_service_delegate_->server_revokes_.erase(std::find_if(
      token_service_delegate_->server_revokes_.begin(),
      token_service_delegate_->server_revokes_.end(),
      [this](const std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate::
                                       RevokeServerRefreshToken>& item) {
        return item.get() == this;
      }));
}

MutableProfileOAuth2TokenServiceDelegate::AccountStatus::AccountStatus(
    SigninErrorController* signin_error_controller,
    const std::string& account_id,
    const std::string& refresh_token)
    : signin_error_controller_(signin_error_controller),
      account_id_(account_id),
      refresh_token_(refresh_token),
      last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(signin_error_controller_);
  DCHECK(!account_id_.empty());
  signin_error_controller_->AddProvider(this);
}

MutableProfileOAuth2TokenServiceDelegate::AccountStatus::~AccountStatus() {
  signin_error_controller_->RemoveProvider(this);
}

void MutableProfileOAuth2TokenServiceDelegate::AccountStatus::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    signin_error_controller_->AuthStatusChanged();
  }
}

std::string
MutableProfileOAuth2TokenServiceDelegate::AccountStatus::GetAccountId() const {
  return account_id_;
}

GoogleServiceAuthError
MutableProfileOAuth2TokenServiceDelegate::AccountStatus::GetAuthStatus() const {
  return last_auth_error_;
}

MutableProfileOAuth2TokenServiceDelegate::
    MutableProfileOAuth2TokenServiceDelegate(
        SigninClient* client,
        SigninErrorController* signin_error_controller,
        AccountTrackerService* account_tracker_service)
    : web_data_service_request_(0),
      load_credentials_state_(LOAD_CREDENTIALS_NOT_STARTED),
      backoff_entry_(&backoff_policy_),
      backoff_error_(GoogleServiceAuthError::NONE),
      client_(client),
      signin_error_controller_(signin_error_controller),
      account_tracker_service_(account_tracker_service) {
  VLOG(1) << "MutablePO2TS::MutablePO2TS";
  DCHECK(client);
  DCHECK(signin_error_controller);
  // It's okay to fill the backoff policy after being used in construction.
  backoff_policy_.num_errors_to_ignore = 0;
  backoff_policy_.initial_delay_ms = 1000;
  backoff_policy_.multiply_factor = 2.0;
  backoff_policy_.jitter_factor = 0.2;
  backoff_policy_.maximum_backoff_ms = 15 * 60 * 1000;
  backoff_policy_.entry_lifetime_ms = -1;
  backoff_policy_.always_use_initial_delay = false;
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

MutableProfileOAuth2TokenServiceDelegate::
    ~MutableProfileOAuth2TokenServiceDelegate() {
  VLOG(1) << "MutablePO2TS::~MutablePO2TS";
  DCHECK(server_revokes_.empty());
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

OAuth2AccessTokenFetcher*
MutableProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  ValidateAccountId(account_id);
  // check whether the account has persistent error.
  if (refresh_tokens_[account_id]->GetAuthStatus().IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << refresh_tokens_[account_id]->GetAuthStatus().state();
    return new OAuth2AccessTokenFetcherImmediateError(
        consumer, refresh_tokens_[account_id]->GetAuthStatus());
  }
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    return new OAuth2AccessTokenFetcherImmediateError(consumer, backoff_error_);
  }
  std::string refresh_token = GetRefreshToken(account_id);
  DCHECK(!refresh_token.empty());
  return new OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token);
}

bool MutableProfileOAuth2TokenServiceDelegate::RefreshTokenHasError(
    const std::string& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  return it == refresh_tokens_.end() ? false
                                     : IsError(it->second->GetAuthStatus());
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "MutablePO2TS::UpdateAuthError. Error: " << error.state();
  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.  We do keep it around though
  // to report for future requests being denied for "backoff" reasons.
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  if (refresh_tokens_.count(account_id) == 0) {
    // This could happen if the preferences have been corrupted (see
    // http://crbug.com/321370). In a Debug build that would be a bug, but in a
    // Release build we want to deal with it gracefully.
    NOTREACHED();
    return;
  }
  refresh_tokens_[account_id]->SetLastAuthError(error);
}

bool MutableProfileOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  VLOG(1) << "MutablePO2TS::RefreshTokenIsAvailable";
  return !GetRefreshToken(account_id).empty();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshToken(
    const std::string& account_id) const {
  AccountStatusMap::const_iterator iter = refresh_tokens_.find(account_id);
  if (iter != refresh_tokens_.end())
    return iter->second->refresh_token();
  return std::string();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshTokenForTest(
    const std::string& account_id) const {
  return GetRefreshToken(account_id);
}

std::vector<std::string>
MutableProfileOAuth2TokenServiceDelegate::GetAccounts() {
  std::vector<std::string> account_ids;
  for (auto& token : refresh_tokens_) {
    account_ids.push_back(token.first);
  }
  return account_ids;
}

net::URLRequestContextGetter*
MutableProfileOAuth2TokenServiceDelegate::GetRequestContext() const {
  return client_->GetURLRequestContext();
}

OAuth2TokenServiceDelegate::LoadCredentialsState
MutableProfileOAuth2TokenServiceDelegate::GetLoadCredentialsState() const {
  return load_credentials_state_;
}

void MutableProfileOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  if (load_credentials_state_ == LOAD_CREDENTIALS_IN_PROGRESS) {
    VLOG(1) << "Load credentials operation already in progress";
    return;
  }

  load_credentials_state_ = LOAD_CREDENTIALS_IN_PROGRESS;
  if (primary_account_id.empty() &&
      !signin::IsAccountConsistencyDiceEnabled()) {
    load_credentials_state_ = LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS;
    FireRefreshTokensLoaded();
    return;
  }

  if (!primary_account_id.empty())
    ValidateAccountId(primary_account_id);
  DCHECK(loading_primary_account_id_.empty());
  DCHECK_EQ(0, web_data_service_request_);

  refresh_tokens_.clear();

  scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
  if (!token_web_data) {
    // This case only exists in unit tests that do not care about loading
    // credentials.
    load_credentials_state_ = LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS;
    FireRefreshTokensLoaded();
    return;
  }

  if (!primary_account_id.empty()) {
    // If the account_id is an email address, then canonicalize it.  This
    // is to support legacy account_ids, and will not be needed after
    // switching to gaia-ids.
    if (primary_account_id.find('@') != std::string::npos) {
      loading_primary_account_id_ = gaia::CanonicalizeEmail(primary_account_id);
    } else {
      loading_primary_account_id_ = primary_account_id;
    }
  }

  web_data_service_request_ = token_web_data->GetAllTokens(this);
}

void MutableProfileOAuth2TokenServiceDelegate::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  VLOG(1) << "MutablePO2TS::OnWebDataServiceRequestDone. Result type: "
          << (result.get() == nullptr ? -1
                                      : static_cast<int>(result->GetType()));

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 MutableProfileOAuth2Token...::OnWebDataServiceRequestDone"));

  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());
    LoadAllCredentialsIntoMemory(token_result->GetValue().tokens);
    load_credentials_state_ =
        LoadCredentialsStateFromTokenResult(token_result->GetValue().db_result);
  } else {
    load_credentials_state_ = LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS;
  }
  FireRefreshTokensLoaded();

  // Make sure that we have an entry for |loading_primary_account_id_| in the
  // map.  The entry could be missing if there is a corruption in the token DB
  // while this profile is connected to an account.
  DCHECK(!loading_primary_account_id_.empty() ||
         signin::IsAccountConsistencyDiceEnabled());
  if (!loading_primary_account_id_.empty() &&
      refresh_tokens_.count(loading_primary_account_id_) == 0) {
    refresh_tokens_[loading_primary_account_id_].reset(new AccountStatus(
        signin_error_controller_, loading_primary_account_id_, std::string()));
  }

  // If we don't have a refresh token for a known account, signal an error.
  for (auto& token : refresh_tokens_) {
    if (!RefreshTokenIsAvailable(token.first)) {
      UpdateAuthError(token.first,
                      GoogleServiceAuthError(
                          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      break;
    }
  }

  loading_primary_account_id_.clear();
}

void MutableProfileOAuth2TokenServiceDelegate::LoadAllCredentialsIntoMemory(
    const std::map<std::string, std::string>& db_tokens) {
  std::string old_login_token;

  {
    ScopedBatchChange batch(this);

    VLOG(1) << "MutablePO2TS::LoadAllCredentialsIntoMemory; "
            << db_tokens.size() << " Credential(s).";
    AccountTrackerService::AccountIdMigrationState migration_state =
        account_tracker_service_->GetMigrationState();
    for (std::map<std::string, std::string>::const_iterator iter =
             db_tokens.begin();
         iter != db_tokens.end(); ++iter) {
      std::string prefixed_account_id = iter->first;
      std::string refresh_token = iter->second;

      if (IsLegacyRefreshTokenId(prefixed_account_id) && !refresh_token.empty())
        old_login_token = refresh_token;

      if (IsLegacyServiceId(prefixed_account_id)) {
        scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
        if (token_web_data.get())
          token_web_data->RemoveTokenForService(prefixed_account_id);
      } else {
        DCHECK(!refresh_token.empty());
        std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);

        if (migration_state == AccountTrackerService::MIGRATION_IN_PROGRESS) {
          // Migrate to gaia-ids.
          AccountInfo account_info =
              account_tracker_service_->FindAccountInfoByEmail(account_id);
          // |account_info.gaia| could be empty if |account_id| is already gaia
          // id. This could happen if the chrome was closed in the middle of
          // migration.
          if (!account_info.gaia.empty()) {
            PersistCredentials(account_info.gaia, refresh_token);
            ClearPersistedCredentials(account_id);
            account_id = account_info.gaia;
          }

          // Skip duplicate accounts, this could happen if migration was
          // crashed in the middle.
          if (refresh_tokens_.count(account_id) != 0)
            continue;
        }

        // If the account_id is an email address, then canonicalize it.  This
        // is to support legacy account_ids, and will not be needed after
        // switching to gaia-ids.
        if (account_id.find('@') != std::string::npos) {
          // If the canonical account id is not the same as the loaded
          // account id, make sure not to overwrite a refresh token from
          // a canonical version.  If no canonical version was loaded, then
          // re-persist this refresh token with the canonical account id.
          std::string canon_account_id = gaia::CanonicalizeEmail(account_id);
          if (canon_account_id != account_id) {
            ClearPersistedCredentials(account_id);
            if (db_tokens.count(ApplyAccountIdPrefix(canon_account_id)) == 0)
              PersistCredentials(canon_account_id, refresh_token);
          }

          account_id = canon_account_id;
        }

        // Only load secondary accounts when account consistency is enabled.
        if (account_id == loading_primary_account_id_ ||
            signin::IsAccountConsistencyDiceEnabled() ||
            signin::IsAccountConsistencyMirrorEnabled()) {
          refresh_tokens_[account_id].reset(new AccountStatus(
              signin_error_controller_, account_id, refresh_token));
          FireRefreshTokenAvailable(account_id);
        } else {
          RevokeCredentialsOnServer(refresh_token);
          ClearPersistedCredentials(account_id);
          FireRefreshTokenRevoked(account_id);
        }
      }
    }

    if (!old_login_token.empty()) {
      DCHECK(!loading_primary_account_id_.empty());
      if (refresh_tokens_.count(loading_primary_account_id_) == 0)
        UpdateCredentials(loading_primary_account_id_, old_login_token);
    }
  }
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());
  ValidateAccountId(account_id);

  signin_metrics::LogSigninAddAccount();

  bool refresh_token_present = refresh_tokens_.count(account_id) > 0;
  if (!refresh_token_present ||
      refresh_tokens_[account_id]->refresh_token() != refresh_token) {
    ScopedBatchChange batch(this);

    // If token present, and different from the new one, cancel its requests,
    // and clear the entries in cache related to that account.
    if (refresh_token_present) {
      VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was present. "
              << "account_id=" << account_id;
      RevokeCredentialsOnServer(refresh_tokens_[account_id]->refresh_token());
      refresh_tokens_[account_id]->set_refresh_token(refresh_token);
    } else {
      VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was absent. "
              << "account_id=" << account_id;
      refresh_tokens_[account_id].reset(new AccountStatus(
          signin_error_controller_, account_id, refresh_token));
    }

    // Save the token in memory and in persistent store.
    PersistCredentials(account_id, refresh_token);

    UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
    FireRefreshTokenAvailable(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::PersistCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
  if (token_web_data.get()) {
    VLOG(1) << "MutablePO2TS::PersistCredentials for account_id=" << account_id;
    token_web_data->SetTokenForService(ApplyAccountIdPrefix(account_id),
                                       refresh_token);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  if (!client_->CanRevokeCredentials())
    return;
  DCHECK(thread_checker_.CalledOnValidThread());

  ScopedBatchChange batch(this);

  VLOG(1) << "MutablePO2TS::RevokeAllCredentials";
  CancelWebTokenFetch();
  AccountStatusMap tokens = refresh_tokens_;
  for (auto& token : tokens)
    RevokeCredentials(token.first);

  DCHECK_EQ(0u, refresh_tokens_.size());

  // Make sure all tokens are removed.
  scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
  if (token_web_data.get())
    token_web_data->RemoveAllTokens();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  ValidateAccountId(account_id);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (refresh_tokens_.count(account_id) > 0) {
    VLOG(1) << "MutablePO2TS::RevokeCredentials for account_id=" << account_id;
    ScopedBatchChange batch(this);
    RevokeCredentialsOnServer(refresh_tokens_[account_id]->refresh_token());
    refresh_tokens_.erase(account_id);
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::ClearPersistedCredentials(
    const std::string& account_id) {
  scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
  if (token_web_data.get()) {
    VLOG(1) << "MutablePO2TS::ClearPersistedCredentials for account_id="
            << account_id;
    token_web_data->RemoveTokenForService(ApplyAccountIdPrefix(account_id));
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsOnServer(
    const std::string& refresh_token) {
  // Keep track or all server revoke requests.  This way they can be deleted
  // before the token service is shutdown and won't outlive the profile.
  server_revokes_.push_back(
      base::MakeUnique<RevokeServerRefreshToken>(this, refresh_token));
}

void MutableProfileOAuth2TokenServiceDelegate::CancelWebTokenFetch() {
  if (web_data_service_request_ != 0) {
    scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
    DCHECK(token_web_data.get());
    token_web_data->CancelRequest(web_data_service_request_);
    web_data_service_request_ = 0;
  }
}

void MutableProfileOAuth2TokenServiceDelegate::Shutdown() {
  VLOG(1) << "MutablePO2TS::Shutdown";
  server_revokes_.clear();
  CancelWebTokenFetch();
  refresh_tokens_.clear();
}

void MutableProfileOAuth2TokenServiceDelegate::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // If our network has changed, reset the backoff timer so that errors caused
  // by a previous lack of network connectivity don't prevent new requests.
  backoff_entry_.Reset();
}

const net::BackoffEntry*
    MutableProfileOAuth2TokenServiceDelegate::BackoffEntry() const {
  return &backoff_entry_;
}
