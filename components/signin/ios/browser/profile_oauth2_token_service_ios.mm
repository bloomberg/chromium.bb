// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios.h"

#include <Foundation/Foundation.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "ios/public/provider/components/signin/browser/profile_oauth2_token_service_ios_provider.h"
#include "net/url_request/url_request_status.h"

namespace {

const char* kForceInvalidGrantResponsesRefreshToken =
    "force_invalid_grant_responses_refresh_token";

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
  virtual ~SSOAccessTokenFetcher();

  virtual void Start(const std::string& client_id,
                     const std::string& client_secret,
                     const std::vector<std::string>& scopes) OVERRIDE;

  virtual void CancelRequest() OVERRIDE;

  // Handles an access token response.
  void OnAccessTokenResponse(NSString* token,
                             NSDate* expiration,
                             NSError* error);

 private:
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;
  ios::ProfileOAuth2TokenServiceIOSProvider* provider_;  // weak
  std::string account_id_;
  bool request_was_cancelled_;

  DISALLOW_COPY_AND_ASSIGN(SSOAccessTokenFetcher);
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    ios::ProfileOAuth2TokenServiceIOSProvider* provider,
    const std::string account_id)
    : OAuth2AccessTokenFetcher(consumer),
      weak_factory_(this),
      provider_(provider),
      account_id_(account_id),
      request_was_cancelled_(false) {
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

// Fetcher that returns INVALID_GAIA_CREDENTIALS responses for all requests.
class InvalidGrantAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  explicit InvalidGrantAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer);
  virtual ~InvalidGrantAccessTokenFetcher();

  // OAuth2AccessTokenFetcher
  virtual void Start(const std::string& client_id,
                     const std::string& client_secret,
                     const std::vector<std::string>& scopes) OVERRIDE;
  virtual void CancelRequest() OVERRIDE;

  // Fires token failure notifications with INVALID_GAIA_CREDENTIALS error.
  void FireInvalidGrant();

 private:
  bool request_was_cancelled_;
  DISALLOW_COPY_AND_ASSIGN(InvalidGrantAccessTokenFetcher);
};

InvalidGrantAccessTokenFetcher::InvalidGrantAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer)
    : OAuth2AccessTokenFetcher(consumer),
      request_was_cancelled_(false) {}

InvalidGrantAccessTokenFetcher::~InvalidGrantAccessTokenFetcher() {}

void InvalidGrantAccessTokenFetcher::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&InvalidGrantAccessTokenFetcher::FireInvalidGrant,
                 base::Unretained(this)));
};

void InvalidGrantAccessTokenFetcher::FireInvalidGrant() {
  if (request_was_cancelled_)
    return;
  GoogleServiceAuthError auth_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  FireOnGetTokenFailure(auth_error);
}

void InvalidGrantAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

}  // namespace

ProfileOAuth2TokenServiceIOS::AccountInfo::AccountInfo(
    ProfileOAuth2TokenService* token_service,
    const std::string& account_id)
    : token_service_(token_service),
      account_id_(account_id),
      last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(token_service_);
  DCHECK(!account_id_.empty());
  token_service_->signin_error_controller()->AddProvider(this);
}

ProfileOAuth2TokenServiceIOS::AccountInfo::~AccountInfo() {
  token_service_->signin_error_controller()->RemoveProvider(this);
}

void ProfileOAuth2TokenServiceIOS::AccountInfo::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    token_service_->signin_error_controller()->AuthStatusChanged();
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
    : MutableProfileOAuth2TokenService(),
      use_legacy_token_service_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ProfileOAuth2TokenServiceIOS::~ProfileOAuth2TokenServiceIOS() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProfileOAuth2TokenServiceIOS::Initialize(SigninClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MutableProfileOAuth2TokenService::Initialize(client);
}

void ProfileOAuth2TokenServiceIOS::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelAllRequests();
  accounts_.clear();
  MutableProfileOAuth2TokenService::Shutdown();
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

  use_legacy_token_service_ = !GetProvider()->IsUsingSharedAuthentication();
  if (use_legacy_token_service_) {
    MutableProfileOAuth2TokenService::LoadCredentials(primary_account_id);
    return;
  }

  GetProvider()->InitializeSharedAuthentication();
  ReloadCredentials();
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOS::ReloadCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_legacy_token_service_) {
    NOTREACHED();
    return;
  }

  ScopedBacthChange batch(this);

  // Remove all old accounts that do not appear in |new_accounts| and then
  // load |new_accounts|.
  std::vector<std::string> new_accounts(GetProvider()->GetAllAccountIds());
  std::vector<std::string> old_accounts(GetAccounts());
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
  if (use_legacy_token_service_) {
    MutableProfileOAuth2TokenService::UpdateCredentials(account_id,
                                                        refresh_token);
    return;
  }
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOS::RevokeAllCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_legacy_token_service_) {
    MutableProfileOAuth2TokenService::RevokeAllCredentials();
    return;
  }

  ScopedBacthChange batch(this);
  CancelAllRequests();
  ClearCache();
  AccountInfoMap toRemove = accounts_;
  for (AccountInfoMap::iterator i = toRemove.begin(); i != toRemove.end(); ++i)
    RemoveAccount(i->first);

  DCHECK_EQ(0u, accounts_.size());
}

OAuth2AccessTokenFetcher*
ProfileOAuth2TokenServiceIOS::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  if (use_legacy_token_service_) {
    std::string refresh_token = GetRefreshToken(account_id);
    DCHECK(!refresh_token.empty());
    if (refresh_token == kForceInvalidGrantResponsesRefreshToken) {
      return new InvalidGrantAccessTokenFetcher(consumer);
    } else {
      return MutableProfileOAuth2TokenService::CreateAccessTokenFetcher(
          account_id, getter, consumer);
    }
  }

  return new SSOAccessTokenFetcher(consumer, GetProvider(), account_id);
}

void ProfileOAuth2TokenServiceIOS::ForceInvalidGrantResponses() {
  if (!use_legacy_token_service_) {
    NOTREACHED();
    return;
  }
  std::vector<std::string> accounts =
      MutableProfileOAuth2TokenService::GetAccounts();
  if (accounts.empty()) {
    NOTREACHED();
    return;
  }

  std::string first_account_id = *accounts.begin();
  if (RefreshTokenIsAvailable(first_account_id) &&
      GetRefreshToken(first_account_id) !=
          kForceInvalidGrantResponsesRefreshToken) {
    MutableProfileOAuth2TokenService::RevokeAllCredentials();
  }

  ScopedBacthChange batch(this);
  for (auto i = accounts.begin(); i != accounts.end(); ++i) {
    std::string account_id = *i;
    MutableProfileOAuth2TokenService::UpdateCredentials(
        account_id,
        kForceInvalidGrantResponsesRefreshToken);
  }
}

void ProfileOAuth2TokenServiceIOS::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Call |MutableProfileOAuth2TokenService::InvalidateOAuth2Token| to clear the
  // cached access token.
  MutableProfileOAuth2TokenService::InvalidateOAuth2Token(account_id,
                                                          client_id,
                                                          scopes,
                                                          access_token);

  // There is no need to inform the authentication library that the access
  // token is invalid as it never caches the token.
}

std::vector<std::string> ProfileOAuth2TokenServiceIOS::GetAccounts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_legacy_token_service_) {
    return MutableProfileOAuth2TokenService::GetAccounts();
  }

  std::vector<std::string> account_ids;
  for (auto i = accounts_.begin(); i != accounts_.end(); ++i)
    account_ids.push_back(i->first);
  return account_ids;
}

bool ProfileOAuth2TokenServiceIOS::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (use_legacy_token_service_) {
    return MutableProfileOAuth2TokenService::RefreshTokenIsAvailable(
        account_id);
  }

  return accounts_.count(account_id) > 0;
}

std::string ProfileOAuth2TokenServiceIOS::GetRefreshToken(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_legacy_token_service_)
    return MutableProfileOAuth2TokenService::GetRefreshToken(account_id);

  // On iOS, the refresh token does not exist as ProfileOAuth2TokenServiceIOS
  // fetches the access token from the iOS authentication library.
  NOTREACHED();
  return std::string();
}

std::string
ProfileOAuth2TokenServiceIOS::GetRefreshTokenWhenNotUsingSharedAuthentication(
    const std::string& account_id) {
  DCHECK(use_legacy_token_service_);
  return GetRefreshToken(account_id);
}

void ProfileOAuth2TokenServiceIOS::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (use_legacy_token_service_) {
    MutableProfileOAuth2TokenService::UpdateAuthError(account_id, error);
    return;
  }

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
  DCHECK(!use_legacy_token_service_);

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
    accounts_[account_id].reset(new AccountInfo(this, account_id));
  }
  UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOS::RemoveAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!use_legacy_token_service_);

  if (accounts_.count(account_id) > 0) {
    CancelRequestsForAccount(account_id);
    ClearCacheForAccount(account_id);
    accounts_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void ProfileOAuth2TokenServiceIOS::StartUsingSharedAuthentication() {
  if (!use_legacy_token_service_)
    return;
  MutableProfileOAuth2TokenService::RevokeAllCredentials();
  use_legacy_token_service_ = false;
}

void ProfileOAuth2TokenServiceIOS::SetUseLegacyTokenServiceForTesting(
    bool use_legacy_token_service) {
  use_legacy_token_service_ = use_legacy_token_service;
}
