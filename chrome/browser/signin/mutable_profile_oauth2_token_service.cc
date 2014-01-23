// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/signin/core/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

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

// This class sends a request to GAIA to revoke the given refresh token from
// the server.  This is a best effort attempt only.  This class deletes itself
// when done sucessfully or otherwise.
class RevokeServerRefreshToken : public GaiaAuthConsumer {
 public:
  RevokeServerRefreshToken(const std::string& account_id,
                           net::URLRequestContextGetter* request_context);
  virtual ~RevokeServerRefreshToken();

 private:
  // GaiaAuthConsumer overrides:
  virtual void OnOAuth2RevokeTokenCompleted() OVERRIDE;

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  GaiaAuthFetcher fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RevokeServerRefreshToken);
};

RevokeServerRefreshToken::RevokeServerRefreshToken(
    const std::string& refresh_token,
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context),
      fetcher_(this, GaiaConstants::kChromeSource, request_context) {
  fetcher_.StartRevokeOAuth2Token(refresh_token);
}

RevokeServerRefreshToken::~RevokeServerRefreshToken() {}

void RevokeServerRefreshToken::OnOAuth2RevokeTokenCompleted() {
  delete this;
}

}  // namespace

MutableProfileOAuth2TokenService::AccountInfo::AccountInfo(
    ProfileOAuth2TokenService* token_service,
    const std::string& account_id,
    const std::string& refresh_token)
  : token_service_(token_service),
    account_id_(account_id),
    refresh_token_(refresh_token),
    last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(token_service_);
  DCHECK(!account_id_.empty());
  token_service_->signin_global_error()->AddProvider(this);
}

MutableProfileOAuth2TokenService::AccountInfo::~AccountInfo() {
  token_service_->signin_global_error()->RemoveProvider(this);
}

void MutableProfileOAuth2TokenService::AccountInfo::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    token_service_->signin_global_error()->AuthStatusChanged();
  }
}

std::string
MutableProfileOAuth2TokenService::AccountInfo::GetAccountId() const {
  return account_id_;
}

GoogleServiceAuthError
MutableProfileOAuth2TokenService::AccountInfo::GetAuthStatus() const {
  return last_auth_error_;
}

MutableProfileOAuth2TokenService::MutableProfileOAuth2TokenService()
    : web_data_service_request_(0)  {
}

MutableProfileOAuth2TokenService::~MutableProfileOAuth2TokenService() {
}

void MutableProfileOAuth2TokenService::Shutdown() {
  CancelWebTokenFetch();
  CancelAllRequests();
  refresh_tokens_.clear();

  ProfileOAuth2TokenService::Shutdown();
}

std::string MutableProfileOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) {
  AccountInfoMap::const_iterator iter = refresh_tokens_.find(account_id);
  if (iter != refresh_tokens_.end())
    return iter->second->refresh_token();
  return std::string();
}

net::URLRequestContextGetter*
MutableProfileOAuth2TokenService::GetRequestContext() {
  return profile()->GetRequestContext();
}

void MutableProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK(!primary_account_id.empty());
  DCHECK(loading_primary_account_id_.empty());
  DCHECK_EQ(0, web_data_service_request_);

  CancelAllRequests();
  refresh_tokens().clear();
  loading_primary_account_id_ = primary_account_id;
  scoped_refptr<TokenWebData> token_web_data =
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile(), Profile::EXPLICIT_ACCESS);
  if (token_web_data.get())
    web_data_service_request_ = token_web_data->GetAllTokens(this);
}

void MutableProfileOAuth2TokenService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    const WDTypedResult* result) {
  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<std::map<std::string, std::string> > * token_result =
        static_cast<const WDResult<std::map<std::string, std::string> > * > (
            result);
    LoadAllCredentialsIntoMemory(token_result->GetValue());
  }

  // Make sure that we have an entry for |loading_primary_account_id_| in the
  // map.  The entry could be missing if there is a corruption in the token DB
  // while this profile is connected to an account.
  DCHECK(!loading_primary_account_id_.empty());
  if (refresh_tokens().count(loading_primary_account_id_) == 0) {
    refresh_tokens()[loading_primary_account_id_].reset(
        new AccountInfo(this, loading_primary_account_id_, std::string()));
  }

  // If we don't have a refresh token for a known account, signal an error.
  for (AccountInfoMap::const_iterator i = refresh_tokens_.begin();
       i != refresh_tokens_.end(); ++i) {
    if (!RefreshTokenIsAvailable(i->first)) {
      UpdateAuthError(
          i->first,
          GoogleServiceAuthError(
              GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      break;
    }
  }

  loading_primary_account_id_.clear();
}

void MutableProfileOAuth2TokenService::LoadAllCredentialsIntoMemory(
    const std::map<std::string, std::string>& db_tokens) {
  std::string old_login_token;

  for (std::map<std::string, std::string>::const_iterator iter =
           db_tokens.begin();
       iter != db_tokens.end();
       ++iter) {
    std::string prefixed_account_id = iter->first;
    std::string refresh_token = iter->second;

    if (IsLegacyRefreshTokenId(prefixed_account_id) && !refresh_token.empty())
      old_login_token = refresh_token;

    if (IsLegacyServiceId(prefixed_account_id)) {
      scoped_refptr<TokenWebData> token_web_data =
          WebDataServiceFactory::GetTokenWebDataForProfile(
              profile(), Profile::EXPLICIT_ACCESS);
      if (token_web_data.get())
        token_web_data->RemoveTokenForService(prefixed_account_id);
    } else {
      DCHECK(!refresh_token.empty());
      std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);
      refresh_tokens()[account_id].reset(
          new AccountInfo(this, account_id, refresh_token));
      FireRefreshTokenAvailable(account_id);
      // TODO(fgorski): Notify diagnostic observers.
    }
  }

  if (!old_login_token.empty()) {
    DCHECK(!loading_primary_account_id_.empty());
    if (refresh_tokens().count(loading_primary_account_id_) == 0)
      UpdateCredentials(loading_primary_account_id_, old_login_token);
  }

  FireRefreshTokensLoaded();
}

void MutableProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE)
    return;

#if defined(OS_IOS)
  // ProfileOauth2TokenService does not manage the refresh tokens on iOS - the
  // account info on iOS is only used to manage the authentication error state.
  // Simply add an account info entry with empty refresh token if none exists.
  if (refresh_tokens_.count(account_id) == 0) {
      refresh_tokens_[account_id].reset(
          new AccountInfo(this, account_id, std::string()));
  }
#endif

  if (refresh_tokens_.count(account_id) == 0) {
    // This could happen if the preferences have been corrupted (see
    // http://crbug.com/321370). In a Debug build that would be a bug, but in a
    // Release build we want to deal with it gracefully.
    NOTREACHED();
    return;
  }
  refresh_tokens_[account_id]->SetLastAuthError(error);
}

std::vector<std::string> MutableProfileOAuth2TokenService::GetAccounts() {
  std::vector<std::string> account_ids;
  for (AccountInfoMap::const_iterator iter = refresh_tokens_.begin();
           iter != refresh_tokens_.end(); ++iter) {
    account_ids.push_back(iter->first);
  }
  return account_ids;
}

void MutableProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  bool refresh_token_present = refresh_tokens_.count(account_id) > 0;
  if (!refresh_token_present ||
      refresh_tokens_[account_id]->refresh_token() != refresh_token) {
    // If token present, and different from the new one, cancel its requests,
    // and clear the entries in cache related to that account.
    if (refresh_token_present) {
      RevokeCredentialsOnServer(refresh_tokens_[account_id]->refresh_token());
      CancelRequestsForAccount(account_id);
      ClearCacheForAccount(account_id);
      refresh_tokens_[account_id]->set_refresh_token(refresh_token);
    } else {
      refresh_tokens_[account_id].reset(
          new AccountInfo(this, account_id, refresh_token));
    }

    // Save the token in memory and in persistent store.
    PersistCredentials(account_id, refresh_token);

    UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
    FireRefreshTokenAvailable(account_id);
  }
}

void MutableProfileOAuth2TokenService::RevokeCredentials(
    const std::string& account_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (refresh_tokens_.count(account_id) > 0) {
    RevokeCredentialsOnServer(refresh_tokens_[account_id]->refresh_token());
    CancelRequestsForAccount(account_id);
    ClearCacheForAccount(account_id);
    refresh_tokens_.erase(account_id);
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void MutableProfileOAuth2TokenService::PersistCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  scoped_refptr<TokenWebData> token_web_data =
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile(), Profile::EXPLICIT_ACCESS);
  if (token_web_data.get()) {
    token_web_data->SetTokenForService(ApplyAccountIdPrefix(account_id),
                                       refresh_token);
  }
}

void MutableProfileOAuth2TokenService::ClearPersistedCredentials(
    const std::string& account_id) {
  scoped_refptr<TokenWebData> token_web_data =
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile(), Profile::EXPLICIT_ACCESS);
  if (token_web_data.get())
    token_web_data->RemoveTokenForService(ApplyAccountIdPrefix(account_id));
}

void MutableProfileOAuth2TokenService::RevokeAllCredentials() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CancelWebTokenFetch();
  CancelAllRequests();
  ClearCache();
  AccountInfoMap tokens = refresh_tokens_;
  for (AccountInfoMap::iterator i = tokens.begin(); i != tokens.end(); ++i)
    RevokeCredentials(i->first);

  DCHECK_EQ(0u, refresh_tokens_.size());
}

void MutableProfileOAuth2TokenService::RevokeCredentialsOnServer(
    const std::string& refresh_token) {
  // RevokeServerRefreshToken deletes itself when done.
  new RevokeServerRefreshToken(refresh_token, GetRequestContext());
}

void MutableProfileOAuth2TokenService::CancelWebTokenFetch() {
  if (web_data_service_request_ != 0) {
    scoped_refptr<TokenWebData> token_web_data =
        WebDataServiceFactory::GetTokenWebDataForProfile(
            profile(), Profile::EXPLICIT_ACCESS);
    DCHECK(token_web_data.get());
    token_web_data->CancelRequest(web_data_service_request_);
    web_data_service_request_  = 0;
  }
}
