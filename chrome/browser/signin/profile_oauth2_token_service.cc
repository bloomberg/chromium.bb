// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/webdata/token_web_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kAccountIdPrefix[] = "AccountId-";
const size_t kAccountIdPrefixLength = 10;

bool IsLegacyServiceId(const std::string& account_id) {
  return account_id.compare(0u, kAccountIdPrefixLength, kAccountIdPrefix) != 0;
}

bool IsLegacyRefreshTokenId(const std::string& service_id) {
  return service_id == GaiaConstants::kGaiaOAuth2LoginRefreshToken;
}

std::string ApplyAccountIdPrefix(const std::string& account_id) {
  return kAccountIdPrefix + account_id;
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(kAccountIdPrefixLength);
}

}  // namespace

ProfileOAuth2TokenService::ProfileOAuth2TokenService()
    : profile_(NULL),
      web_data_service_request_(0),
      last_auth_error_(GoogleServiceAuthError::NONE) {
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  DCHECK(!signin_global_error_.get()) <<
      "ProfileOAuth2TokenService::Initialize called but not "
      "ProfileOAuth2TokenService::Shutdown";
}

void ProfileOAuth2TokenService::Initialize(Profile* profile) {
  DCHECK(profile);
  DCHECK(!profile_);
  profile_ = profile;

  signin_global_error_.reset(new SigninGlobalError(profile));
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      signin_global_error_.get());
  signin_global_error_->AddProvider(this);

  content::Source<TokenService> token_service_source(
      TokenServiceFactory::GetForProfile(profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKENS_CLEARED,
                 token_service_source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 token_service_source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                 token_service_source);
}

void ProfileOAuth2TokenService::Shutdown() {
  DCHECK(profile_) << "Shutdown() called without matching call to Initialize()";
  CancelAllRequests();
  signin_global_error_->RemoveProvider(this);
  GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
      signin_global_error_.get());
  signin_global_error_.reset();
}

std::string ProfileOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) {
  std::map<std::string, std::string>::const_iterator iter =
      refresh_tokens_.find(account_id);
  if (iter != refresh_tokens_.end())
    return iter->second;
  return std::string();
}

net::URLRequestContextGetter* ProfileOAuth2TokenService::GetRequestContext() {
  return profile_->GetRequestContext();
}

void ProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // TODO(fgorski): SigninGlobalError needs to be made multi-login aware.
  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED)
    return;

  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    signin_global_error_->AuthStatusChanged();
  }
}

void ProfileOAuth2TokenService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const std::string& account_id = GetPrimaryAccountId();
  switch (type) {
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      TokenService::TokenAvailableDetails* tok_details =
          content::Details<TokenService::TokenAvailableDetails>(details).ptr();
      if (tok_details->service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        // TODO(fgorski): Work on removing this code altogether in favor of the
        // upgrade steps invoked by Initialize.
        // TODO(fgorski): Refresh token received that way is not persisted in
        // the token DB.
        CancelRequestsForAccount(account_id);
        ClearCacheForAccount(account_id);
        refresh_tokens_[account_id] = tok_details->token();
        UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
        FireRefreshTokenAvailable(account_id);
      }
      break;
    }
    case chrome::NOTIFICATION_TOKENS_CLEARED: {
      CancelAllRequests();
      ClearCache();
      UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
      break;
    }
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED:
      // During startup, if the user is signed in and the OAuth2 refresh token
      // is empty, flag it as an error by badging the menu. Otherwise, if the
      // user goes on to set up sync, they will have to make two attempts:
      // One to surface the OAuth2 error, and a second one after signing in.
      // See crbug.com/276650.
      if (!account_id.empty() && !RefreshTokenIsAvailable(account_id)) {
        UpdateAuthError(account_id, GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      }
      FireRefreshTokensLoaded();
      break;
    default:
      NOTREACHED() << "Invalid notification type=" << type;
      break;
  }
}

GoogleServiceAuthError ProfileOAuth2TokenService::GetAuthStatus() const {
  return last_auth_error_;
}

std::string ProfileOAuth2TokenService::GetPrimaryAccountId() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);
  // TODO(fgorski): DCHECK(signin_manager) here - it may require update to test
  // code and the line above (SigninManager might not exist yet).
  return signin_manager ? signin_manager->GetAuthenticatedUsername()
      : std::string();
}

std::vector<std::string> ProfileOAuth2TokenService::GetAccounts() {
  std::vector<std::string> account_ids;
  for (std::map<std::string, std::string>::const_iterator iter =
           refresh_tokens_.begin(); iter != refresh_tokens_.end(); ++iter) {
    account_ids.push_back(iter->first);
  }
  return account_ids;
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!refresh_token.empty());

  bool refresh_token_present = refresh_tokens_.count(account_id) > 0;
  if (!refresh_token_present ||
      refresh_tokens_[account_id] != refresh_token) {
    // If token present, and different from the new one, cancel its requests,
    // and clear the entries in cache related to that account.
    if (refresh_token_present) {
      CancelRequestsForAccount(account_id);
      ClearCacheForAccount(account_id);
    }

    // Save the token in memory and in persistent store.
    refresh_tokens_[account_id] = refresh_token;
    PersistCredentials(account_id, refresh_token);

    UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
    FireRefreshTokenAvailable(account_id);
    // TODO(fgorski): Notify diagnostic observers.
  }
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const std::string& account_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (refresh_tokens_.count(account_id) > 0) {
    CancelRequestsForAccount(account_id);
    ClearCacheForAccount(account_id);
    refresh_tokens_.erase(account_id);
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);

    // TODO(fgorski): Notify diagnostic observers.
  }
}

void ProfileOAuth2TokenService::PersistCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile_);
  if (token_web_data.get()) {
    token_web_data->SetTokenForService(ApplyAccountIdPrefix(account_id),
                                       refresh_token);
  }
}

void ProfileOAuth2TokenService::ClearPersistedCredentials(
    const std::string& account_id) {
  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile_);
  if (token_web_data.get())
    token_web_data->RemoveTokenForService(ApplyAccountIdPrefix(account_id));
}

void ProfileOAuth2TokenService::RevokeAllCredentials() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  CancelAllRequests();
  for (std::map<std::string, std::string>::const_iterator iter =
           refresh_tokens_.begin();
       iter != refresh_tokens_.end();
       ++iter) {
    FireRefreshTokenRevoked(iter->first);
  }
  refresh_tokens_.clear();

  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile_);
  if (token_web_data.get())
    token_web_data->RemoveAllTokens();

  // TODO(fgorski): Notify diagnostic observers.
}

void ProfileOAuth2TokenService::LoadCredentials() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(0, web_data_service_request_);

  CancelAllRequests();
  refresh_tokens_.clear();
  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile_);
  if (token_web_data.get())
    web_data_service_request_ = token_web_data->GetAllTokens(this);
}

void ProfileOAuth2TokenService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    const WDTypedResult* result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<std::map<std::string, std::string> > * token_result =
        static_cast<const WDResult<std::map<std::string, std::string> > * > (
            result);
    LoadAllCredentialsIntoMemory(token_result->GetValue());
  }
}

void ProfileOAuth2TokenService::LoadAllCredentialsIntoMemory(
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
          TokenWebData::FromBrowserContext(profile_);
      if (token_web_data.get())
        token_web_data->RemoveTokenForService(prefixed_account_id);
    } else {
      DCHECK(!refresh_token.empty());
      std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);
      refresh_tokens_[account_id] = refresh_token;
      FireRefreshTokenAvailable(account_id);
      // TODO(fgorski): Notify diagnostic observers.
    }
  }

  if (!old_login_token.empty() &&
      refresh_tokens_.count(GetPrimaryAccountId()) == 0) {
    UpdateCredentials(GetPrimaryAccountId(), old_login_token);
  }

  FireRefreshTokensLoaded();
}
