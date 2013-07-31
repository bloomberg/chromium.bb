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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

std::string GetAccountId(Profile* profile) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);
  return signin_manager ? signin_manager->GetAuthenticatedUsername() :
      std::string();
}

}  // namespace

ProfileOAuth2TokenService::ProfileOAuth2TokenService()
    : profile_(NULL),
      last_auth_error_(GoogleServiceAuthError::NONE) {
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  DCHECK(!signin_global_error_.get()) <<
      "ProfileOAuth2TokenService::Initialize called but not "
      "ProfileOAuth2TokenService::Shutdown";
}

void ProfileOAuth2TokenService::Initialize(Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 token_service_source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                 token_service_source);
}

void ProfileOAuth2TokenService::Shutdown() {
  CancelAllRequests();
  signin_global_error_->RemoveProvider(this);
  GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
      signin_global_error_.get());
  signin_global_error_.reset();
}

std::string ProfileOAuth2TokenService::GetRefreshToken() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (!token_service || !token_service->HasOAuthLoginToken()) {
    return std::string();
  }
  return token_service->GetOAuth2LoginRefreshToken();
}

void ProfileOAuth2TokenService::UpdateAuthError(
    const GoogleServiceAuthError& error) {
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
  switch (type) {
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      TokenService::TokenAvailableDetails* tok_details =
          content::Details<TokenService::TokenAvailableDetails>(details).ptr();
      if (tok_details->service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        // TODO(fgorski): Canceling all requests will not be correct in a
        // multi-login environment. We should cancel only the requests related
        // to the token being replaced (old token for the same account_id).
        // Previous refresh token is not available at this point, but since
        // there are no other refresh tokens, we cancel all active requests.
        CancelAllRequests();
        ClearCache();
        UpdateAuthError(GoogleServiceAuthError::AuthErrorNone());
        FireRefreshTokenAvailable(GetAccountId(profile_));
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_REQUEST_FAILED: {
      TokenService::TokenRequestFailedDetails* tok_details =
          content::Details<TokenService::TokenRequestFailedDetails>(details)
              .ptr();
      if (tok_details->service() == GaiaConstants::kLSOService ||
          tok_details->service() ==
              GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        // TODO(fgorski): Canceling all requests will not be correct in a
        // multi-login environment. We should cacnel only the requests related
        // to the failed refresh token.
        // Failed refresh token is not available at this point, but since
        // there are no other refresh tokens, we cancel all active requests.
        CancelAllRequests();
        ClearCache();
        UpdateAuthError(tok_details->error());
        FireRefreshTokenRevoked(GetAccountId(profile_), tok_details->error());
      }
      break;
    }
    case chrome::NOTIFICATION_TOKENS_CLEARED: {
      CancelAllRequests();
      ClearCache();
      UpdateAuthError(GoogleServiceAuthError::AuthErrorNone());
      FireRefreshTokensCleared();
      break;
    }
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED:
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

net::URLRequestContextGetter* ProfileOAuth2TokenService::GetRequestContext() {
  return profile_->GetRequestContext();
}

void ProfileOAuth2TokenService::RegisterCacheEntry(
    const std::string& refresh_token,
    const ScopeSet& scopes,
    const std::string& access_token,
    const base::Time& expiration_date) {
  if (ShouldCacheForRefreshToken(TokenServiceFactory::GetForProfile(profile_),
                                 refresh_token)) {
    OAuth2TokenService::RegisterCacheEntry(refresh_token,
                                           scopes,
                                           access_token,
                                           expiration_date);
  }
}

bool ProfileOAuth2TokenService::ShouldCacheForRefreshToken(
    TokenService *token_service,
    const std::string& refresh_token) {
  if (!token_service ||
      !token_service->HasOAuthLoginToken() ||
      token_service->GetOAuth2LoginRefreshToken().compare(refresh_token) != 0) {
    DLOG(INFO) <<
        "Received a token with a refresh token not maintained by TokenService.";
    return false;
  }
  return true;
}
