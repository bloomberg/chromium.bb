// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"

#include <Foundation/Foundation.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace {

// Match the way Chromium handles authentication errors in
// google_apis/gaia/oauth2_access_token_fetcher.cc:
GoogleServiceAuthError GetGoogleServiceAuthErrorFromNSError(
    ProfileOAuth2TokenServiceIOSProvider* provider,
    NSError* error) {
  if (!error)
    return GoogleServiceAuthError::AuthErrorNone();

  AuthenticationErrorCategory errorCategory =
      provider->GetAuthenticationErrorCategory(error);
  switch (errorCategory) {
    case kAuthenticationErrorCategoryUnknownErrors:
      // Treat all unknown error as unexpected service response errors.
      // This may be too general and may require a finer grain filtering.
      return GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    case kAuthenticationErrorCategoryAuthorizationErrors:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    case kAuthenticationErrorCategoryAuthorizationForbiddenErrors:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exceeded.' (for more details, see
      // google_apis/gaia/oauth2_access_token_fetcher.cc).
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    case kAuthenticationErrorCategoryNetworkServerErrors:
      // Just set the connection error state to FAILED.
      return GoogleServiceAuthError::FromConnectionError(
          net::URLRequestStatus::FAILED);
    case kAuthenticationErrorCategoryUserCancellationErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    case kAuthenticationErrorCategoryUnknownIdentityErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  }
}

class SSOAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  SSOAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                        ProfileOAuth2TokenServiceIOSProvider* provider,
                        const std::string account_id);
  ~SSOAccessTokenFetcher() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(NSString* token,
                             NSDate* expiration,
                             NSError* error);

 private:
  ProfileOAuth2TokenServiceIOSProvider* provider_;  // weak
  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSOAccessTokenFetcher);
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    ProfileOAuth2TokenServiceIOSProvider* provider,
    const std::string account_id)
    : OAuth2AccessTokenFetcher(consumer),
      provider_(provider),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {
  DCHECK(provider_);
}

SSOAccessTokenFetcher::~SSOAccessTokenFetcher() {
}

void SSOAccessTokenFetcher::Start(const std::string& client_id,
                                  const std::string& client_secret,
                                  const std::vector<std::string>& scopes) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      account_id_, client_id, client_secret, scopes_set,
      base::Bind(&SSOAccessTokenFetcher::OnAccessTokenResponse,
                 weak_factory_.GetWeakPtr()));
}

void SSOAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void SSOAccessTokenFetcher::OnAccessTokenResponse(NSString* token,
                                                  NSDate* expiration,
                                                  NSError* error) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  GoogleServiceAuthError auth_error =
      GetGoogleServiceAuthErrorFromNSError(provider_, error);
  if (auth_error.state() == GoogleServiceAuthError::NONE) {
    base::Time expiration_date =
        base::Time::FromDoubleT([expiration timeIntervalSince1970]);
    FireOnGetTokenSuccess(base::SysNSStringToUTF8(token), expiration_date);
  } else {
    FireOnGetTokenFailure(auth_error);
  }
}

}  // namespace

ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::AccountInfo(
    SigninErrorController* signin_error_controller,
    const std::string& account_id)
    : signin_error_controller_(signin_error_controller),
      account_id_(account_id),
      last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(signin_error_controller_);
  DCHECK(!account_id_.empty());
  signin_error_controller_->AddProvider(this);
}

ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::~AccountInfo() {
  signin_error_controller_->RemoveProvider(this);
}

void ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    signin_error_controller_->AuthStatusChanged();
  }
}

std::string ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::GetAccountId()
    const {
  return account_id_;
}

GoogleServiceAuthError
ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::GetAuthStatus() const {
  return last_auth_error_;
}

ProfileOAuth2TokenServiceIOSDelegate::ProfileOAuth2TokenServiceIOSDelegate(
    SigninClient* client,
    ProfileOAuth2TokenServiceIOSProvider* provider,
    SigninErrorController* signin_error_controller)
    : client_(client),
      provider_(provider),
      signin_error_controller_(signin_error_controller) {
  DCHECK(client_);
  DCHECK(provider_);
  DCHECK(signin_error_controller_);
}

ProfileOAuth2TokenServiceIOSDelegate::~ProfileOAuth2TokenServiceIOSDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProfileOAuth2TokenServiceIOSDelegate::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  accounts_.clear();
}

void ProfileOAuth2TokenServiceIOSDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // LoadCredentials() is called iff the user is signed in to Chrome, so the
  // primary account id must not be empty.
  DCHECK(!primary_account_id.empty());

  ReloadCredentials(primary_account_id);
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials(
    const std::string& primary_account_id) {
  DCHECK(!primary_account_id.empty());
  DCHECK(primary_account_id_.empty() ||
         primary_account_id_ == primary_account_id);
  primary_account_id_ = primary_account_id;
  ReloadCredentials();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (primary_account_id_.empty()) {
    // Avoid loading the credentials if there is no primary account id.
    return;
  }

  std::vector<std::string> new_accounts(provider_->GetAllAccountIds());
  if (GetExcludeAllSecondaryAccounts()) {
    // Only keep the |primary_account_id| in the list of new accounts.
    if (std::find(new_accounts.begin(), new_accounts.end(),
                  primary_account_id_) != new_accounts.end()) {
      new_accounts.clear();
      new_accounts.push_back(primary_account_id_);
    }
  } else {
    std::set<std::string> exclude_secondary_accounts =
        GetExcludedSecondaryAccounts();
    DCHECK(std::find(exclude_secondary_accounts.begin(),
                     exclude_secondary_accounts.end(),
                     primary_account_id_) == exclude_secondary_accounts.end());
    for (const auto& excluded_account_id : exclude_secondary_accounts) {
      DCHECK(!excluded_account_id.empty());
      auto account_id_to_remove_position = std::remove(
          new_accounts.begin(), new_accounts.end(), excluded_account_id);
      new_accounts.erase(account_id_to_remove_position, new_accounts.end());
    }
  }

  std::vector<std::string> old_accounts(GetAccounts());
  std::sort(new_accounts.begin(), new_accounts.end());
  std::sort(old_accounts.begin(), old_accounts.end());
  if (new_accounts == old_accounts) {
    // Avoid starting a batch change if there are no changes in the list of
    // account.
    return;
  }

  // Remove all old accounts that do not appear in |new_accounts| and then
  // load |new_accounts|.
  ScopedBatchChange batch(this);
  for (auto i = old_accounts.begin(); i != old_accounts.end(); ++i) {
    if (std::find(new_accounts.begin(), new_accounts.end(), *i) ==
        new_accounts.end()) {
      RemoveAccount(*i);
    }
  }

  // Load all new_accounts.
  for (auto i = new_accounts.begin(); i != new_accounts.end(); ++i) {
    AddOrUpdateAccount(*i);
  }
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOSDelegate::RevokeAllCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ScopedBatchChange batch(this);
  AccountInfoMap toRemove = accounts_;
  for (AccountInfoMap::iterator i = toRemove.begin(); i != toRemove.end(); ++i)
    RemoveAccount(i->first);

  DCHECK_EQ(0u, accounts_.size());
  primary_account_id_.clear();
  ClearExcludedSecondaryAccounts();
}

OAuth2AccessTokenFetcher*
ProfileOAuth2TokenServiceIOSDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  return new SSOAccessTokenFetcher(consumer, provider_, account_id);
}

std::vector<std::string> ProfileOAuth2TokenServiceIOSDelegate::GetAccounts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<std::string> account_ids;
  for (auto i = accounts_.begin(); i != accounts_.end(); ++i)
    account_ids.push_back(i->first);
  return account_ids;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return accounts_.count(account_id) > 0;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenHasError(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = accounts_.find(account_id);
  // TODO(rogerta): should we distinguish between transient and persistent?
  return it == accounts_.end() ? false : IsError(it->second->GetAuthStatus());
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    return;
  }

  if (accounts_.count(account_id) == 0) {
    // Nothing to update as the account has already been removed.
    return;
  }
  accounts_[account_id]->SetLastAuthError(error);
}

// Clear the authentication error state and notify all observers that a new
// refresh token is available so that they request new access tokens.
void ProfileOAuth2TokenServiceIOSDelegate::AddOrUpdateAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  bool account_present = accounts_.count(account_id) > 0;
  if (account_present &&
      accounts_[account_id]->GetAuthStatus().state() ==
          GoogleServiceAuthError::NONE) {
    // No need to update the account if it is already a known account and if
    // there is no auth error.
    return;
  }

  if (!account_present) {
    accounts_[account_id].reset(
        new AccountInfo(signin_error_controller_, account_id));
  }

  UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::RemoveAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  if (accounts_.count(account_id) > 0) {
    accounts_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

std::set<std::string>
ProfileOAuth2TokenServiceIOSDelegate::GetExcludedSecondaryAccounts() {
  const base::ListValue* excluded_secondary_accounts_pref =
      client_->GetPrefs()->GetList(
          prefs::kTokenServiceExcludedSecondaryAccounts);
  std::set<std::string> excluded_secondary_accounts;
  for (base::Value* pref_value : *excluded_secondary_accounts_pref) {
    std::string value;
    if (pref_value->GetAsString(&value))
      excluded_secondary_accounts.insert(value);
  }
  return excluded_secondary_accounts;
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeSecondaryAccounts(
    const std::vector<std::string>& account_ids) {
  for (const auto& account_id : account_ids)
    ExcludeSecondaryAccount(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeSecondaryAccount(
    const std::string& account_id) {
  if (GetExcludeAllSecondaryAccounts()) {
    // Avoid excluding individual secondary accounts when all secondary
    // accounts are excluded.
    return;
  }

  DCHECK(!account_id.empty());
  ListPrefUpdate update(client_->GetPrefs(),
                        prefs::kTokenServiceExcludedSecondaryAccounts);
  base::ListValue* excluded_secondary_accounts = update.Get();
  for (base::Value* pref_value : *excluded_secondary_accounts) {
    std::string value_at_it;
    if (pref_value->GetAsString(&value_at_it) && (value_at_it == account_id)) {
      // |account_id| is already excluded.
      return;
    }
  }
  excluded_secondary_accounts->AppendString(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::IncludeSecondaryAccount(
    const std::string& account_id) {
  if (GetExcludeAllSecondaryAccounts()) {
    // Avoid including individual secondary accounts when all secondary
    // accounts are excluded.
    return;
  }

  DCHECK_NE(account_id, primary_account_id_);
  DCHECK(!primary_account_id_.empty());

  // Excluded secondary account ids is a logical set (not a list) of accounts.
  // As the value stored in the excluded account ids preference is a list,
  // the code below removes all occurences of |account_id| from this list. This
  // ensures that |account_id| is actually included even in cases when the
  // preference value was corrupted (see bug http://crbug.com/453470 as
  // example).
  ListPrefUpdate update(client_->GetPrefs(),
                        prefs::kTokenServiceExcludedSecondaryAccounts);
  base::ListValue* excluded_secondary_accounts = update.Get();
  base::ListValue::iterator it = excluded_secondary_accounts->begin();
  while (it != excluded_secondary_accounts->end()) {
    base::Value* pref_value = *it;
    std::string value_at_it;
    if (pref_value->GetAsString(&value_at_it) && (value_at_it == account_id)) {
      it = excluded_secondary_accounts->Erase(it, nullptr);
      continue;
    }
    ++it;
  }
}

bool ProfileOAuth2TokenServiceIOSDelegate::GetExcludeAllSecondaryAccounts() {
  return client_->GetPrefs()->GetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeAllSecondaryAccounts() {
  client_->GetPrefs()->SetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts, true);
}

void ProfileOAuth2TokenServiceIOSDelegate::ClearExcludedSecondaryAccounts() {
  client_->GetPrefs()->ClearPref(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
  client_->GetPrefs()->ClearPref(prefs::kTokenServiceExcludedSecondaryAccounts);
}
