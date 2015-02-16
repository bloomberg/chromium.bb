// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios.h"

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
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "ios/public/provider/components/signin/browser/profile_oauth2_token_service_ios_provider.h"
#include "net/url_request/url_request_status.h"

namespace {

// Match the way Chromium handles authentication errors in
// google_apis/gaia/oauth2_access_token_fetcher.cc:
GoogleServiceAuthError GetGoogleServiceAuthErrorFromNSError(
    ios::ProfileOAuth2TokenServiceIOSProvider* provider,
    NSError* error) {
  if (!error)
    return GoogleServiceAuthError::AuthErrorNone();

  ios::AuthenticationErrorCategory errorCategory =
      provider->GetAuthenticationErrorCategory(error);
  switch (errorCategory) {
    case ios::kAuthenticationErrorCategoryUnknownErrors:
      // Treat all unknown error as unexpected service response errors.
      // This may be too general and may require a finer grain filtering.
      return GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    case ios::kAuthenticationErrorCategoryAuthorizationErrors:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    case ios::kAuthenticationErrorCategoryAuthorizationForbiddenErrors:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exceeded.' (for more details, see
      // google_apis/gaia/oauth2_access_token_fetcher.cc).
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    case ios::kAuthenticationErrorCategoryNetworkServerErrors:
      // Just set the connection error state to FAILED.
      return GoogleServiceAuthError::FromConnectionError(
          net::URLRequestStatus::FAILED);
    case ios::kAuthenticationErrorCategoryUserCancellationErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    case ios::kAuthenticationErrorCategoryUnknownIdentityErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  }
}

class SSOAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  SSOAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                        ios::ProfileOAuth2TokenServiceIOSProvider* provider,
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
  ios::ProfileOAuth2TokenServiceIOSProvider* provider_;  // weak
  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSOAccessTokenFetcher);
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    ios::ProfileOAuth2TokenServiceIOSProvider* provider,
    const std::string account_id)
    : OAuth2AccessTokenFetcher(consumer),
      provider_(provider),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {
  DCHECK(provider_);
}

SSOAccessTokenFetcher::~SSOAccessTokenFetcher() {}

void SSOAccessTokenFetcher::Start(const std::string& client_id,
                                  const std::string& client_secret,
                                  const std::vector<std::string>& scopes) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      account_id_, client_id, client_secret, scopes_set,
      base::Bind(&SSOAccessTokenFetcher::OnAccessTokenResponse,
                 weak_factory_.GetWeakPtr()));
}

void SSOAccessTokenFetcher::CancelRequest() { request_was_cancelled_ = true; }

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

ProfileOAuth2TokenServiceIOS::AccountInfo::AccountInfo(
    SigninErrorController* signin_error_controller,
    const std::string& account_id)
    : signin_error_controller_(signin_error_controller),
      account_id_(account_id),
      last_auth_error_(GoogleServiceAuthError::NONE),
      marked_for_removal_(false) {
  DCHECK(signin_error_controller_);
  DCHECK(!account_id_.empty());
  signin_error_controller_->AddProvider(this);
}

ProfileOAuth2TokenServiceIOS::AccountInfo::~AccountInfo() {
  signin_error_controller_->RemoveProvider(this);
}

void ProfileOAuth2TokenServiceIOS::AccountInfo::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    signin_error_controller_->AuthStatusChanged();
  }
}

std::string ProfileOAuth2TokenServiceIOS::AccountInfo::GetAccountId() const {
  return account_id_;
}

std::string ProfileOAuth2TokenServiceIOS::AccountInfo::GetUsername() const {
  // TODO(rogerta): when |account_id| becomes the obfuscated gaia id, this
  // will need to be changed.
  return account_id_;
}

GoogleServiceAuthError
ProfileOAuth2TokenServiceIOS::AccountInfo::GetAuthStatus() const {
  return last_auth_error_;
}

ProfileOAuth2TokenServiceIOS::ProfileOAuth2TokenServiceIOS()
    : ProfileOAuth2TokenService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ProfileOAuth2TokenServiceIOS::~ProfileOAuth2TokenServiceIOS() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProfileOAuth2TokenServiceIOS::Initialize(
    SigninClient* client, SigninErrorController* signin_error_controller) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProfileOAuth2TokenService::Initialize(client, signin_error_controller);
}

void ProfileOAuth2TokenServiceIOS::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelAllRequests();
  accounts_.clear();
  ProfileOAuth2TokenService::Shutdown();
}

ios::ProfileOAuth2TokenServiceIOSProvider*
ProfileOAuth2TokenServiceIOS::GetProvider() {
  ios::ProfileOAuth2TokenServiceIOSProvider* provider =
      client()->GetIOSProvider();
  DCHECK(provider);
  return provider;
}

void ProfileOAuth2TokenServiceIOS::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // LoadCredentials() is called iff the user is signed in to Chrome, so the
  // primary account id must not be empty.
  DCHECK(!primary_account_id.empty());

  GetProvider()->InitializeSharedAuthentication();
  ReloadCredentials(primary_account_id);
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOS::ReloadCredentials(
    const std::string& primary_account_id) {
  DCHECK(!primary_account_id.empty());
  DCHECK(primary_account_id_.empty() ||
         primary_account_id_ == primary_account_id);
  primary_account_id_ = primary_account_id;
  ReloadCredentials();
}

void ProfileOAuth2TokenServiceIOS::ReloadCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (primary_account_id_.empty()) {
    // Avoid loading the credentials if there is no primary account id.
    return;
  }

  std::vector<std::string> new_accounts(GetProvider()->GetAllAccountIds());
  if (GetExcludeAllSecondaryAccounts()) {
    // Only keep the |primary_account_id| in the list of new accounts.
    if (std::find(new_accounts.begin(),
                  new_accounts.end(),
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

void ProfileOAuth2TokenServiceIOS::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOS::RevokeAllCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ScopedBatchChange batch(this);
  AccountInfoMap toRemove = accounts_;
  for (AccountInfoMap::iterator i = toRemove.begin(); i != toRemove.end(); ++i)
    RemoveAccount(i->first);

  DCHECK_EQ(0u, accounts_.size());
  primary_account_id_.clear();
  ClearExcludedSecondaryAccounts();
  // |RemoveAccount| should have cancelled all the requests and cleared the
  // cache, account-by-account. This extra-cleaning should do nothing unless
  // something went wrong and some cache values and/or pending requests were not
  // linked to any valid account.
  CancelAllRequests();
  ClearCache();
}

OAuth2AccessTokenFetcher*
ProfileOAuth2TokenServiceIOS::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  return new SSOAccessTokenFetcher(consumer, GetProvider(), account_id);
}

void ProfileOAuth2TokenServiceIOS::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Call |ProfileOAuth2TokenService::InvalidateOAuth2Token| to clear the
  // cached access token.
  ProfileOAuth2TokenService::InvalidateOAuth2Token(account_id,
                                                   client_id,
                                                   scopes,
                                                   access_token);

  // There is no need to inform the authentication library that the access
  // token is invalid as it never caches the token.
}

std::vector<std::string> ProfileOAuth2TokenServiceIOS::GetAccounts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<std::string> account_ids;
  for (auto i = accounts_.begin(); i != accounts_.end(); ++i)
    account_ids.push_back(i->first);
  return account_ids;
}

bool ProfileOAuth2TokenServiceIOS::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  AccountInfoMap::const_iterator iter = accounts_.find(account_id);
  return iter != accounts_.end() && !iter->second->marked_for_removal();
}

void ProfileOAuth2TokenServiceIOS::UpdateAuthError(
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
    NOTREACHED();
    return;
  }
  accounts_[account_id]->SetLastAuthError(error);
}

// Clear the authentication error state and notify all observers that a new
// refresh token is available so that they request new access tokens.
void ProfileOAuth2TokenServiceIOS::AddOrUpdateAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  bool account_present = accounts_.count(account_id) > 0;
  if (account_present && accounts_[account_id]->GetAuthStatus().state() ==
                             GoogleServiceAuthError::NONE) {
    // No need to update the account if it is already a known account and if
    // there is no auth error.
    return;
  }

  if (account_present) {
    CancelRequestsForAccount(account_id);
    ClearCacheForAccount(account_id);
  } else {
    accounts_[account_id].reset(
        new AccountInfo(signin_error_controller(), account_id));
  }
  UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOS::RemoveAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  if (accounts_.count(account_id) > 0) {
    // This is needed to ensure that refresh token for |acccount_id| is not
    // available while the account is removed. Thus all access token requests
    // for |account_id| triggered while an account is being removed will get a
    // user not signed up error response.
    accounts_[account_id]->set_marked_for_removal(true);
    CancelRequestsForAccount(account_id);
    ClearCacheForAccount(account_id);
    accounts_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

std::set<std::string>
ProfileOAuth2TokenServiceIOS::GetExcludedSecondaryAccounts() {
  const base::ListValue* excluded_secondary_accounts_pref =
      client()->GetPrefs()->GetList(
          prefs::kTokenServiceExcludedSecondaryAccounts);
  std::set<std::string> excluded_secondary_accounts;
  for (base::Value* pref_value : *excluded_secondary_accounts_pref) {
    std::string value;
    if (pref_value->GetAsString(&value))
      excluded_secondary_accounts.insert(value);
  }
  return excluded_secondary_accounts;
}

void ProfileOAuth2TokenServiceIOS::ExcludeSecondaryAccounts(
    const std::vector<std::string>& account_ids) {
  for (const auto& account_id : account_ids)
    ExcludeSecondaryAccount(account_id);
}

void ProfileOAuth2TokenServiceIOS::ExcludeSecondaryAccount(
    const std::string& account_id) {
  if (GetExcludeAllSecondaryAccounts()) {
    // Avoid excluding individual secondary accounts when all secondary
    // accounts are excluded.
    return;
  }

  DCHECK(!account_id.empty());
  ListPrefUpdate update(client()->GetPrefs(),
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

void ProfileOAuth2TokenServiceIOS::IncludeSecondaryAccount(
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
  ListPrefUpdate update(client()->GetPrefs(),
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

bool ProfileOAuth2TokenServiceIOS::GetExcludeAllSecondaryAccounts() {
  return client()->GetPrefs()->GetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
}

void ProfileOAuth2TokenServiceIOS::ExcludeAllSecondaryAccounts() {
  client()->GetPrefs()->SetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts, true);
}

void ProfileOAuth2TokenServiceIOS::ClearExcludedSecondaryAccounts() {
  client()->GetPrefs()->ClearPref(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
  client()->GetPrefs()->ClearPref(
      prefs::kTokenServiceExcludedSecondaryAccounts);
}
