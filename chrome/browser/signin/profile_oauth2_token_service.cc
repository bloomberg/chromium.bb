// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    net::URLRequestContextGetter* getter)
    : OAuth2TokenService(getter),
      profile_(NULL),
      last_auth_error_(GoogleServiceAuthError::NONE) {
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
}

void ProfileOAuth2TokenService::Initialize(Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DCHECK(profile);
  DCHECK(!profile_);
  profile_ = profile;

  content::Source<TokenService> token_service_source(
      TokenServiceFactory::GetForProfile(profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKENS_CLEARED,
                 token_service_source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 token_service_source);
  SigninManagerFactory::GetForProfile(profile_)->signin_global_error()->
      AddProvider(this);
}

void ProfileOAuth2TokenService::Shutdown() {
  if (profile_) {
    SigninManagerFactory::GetForProfile(profile_)->signin_global_error()->
        RemoveProvider(this);
  }
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
    SigninManagerFactory::GetForProfile(profile_)->signin_global_error()->
        AuthStatusChanged();
  }
}

void ProfileOAuth2TokenService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKENS_CLEARED ||
         type == chrome::NOTIFICATION_TOKEN_AVAILABLE);
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService::TokenAvailableDetails* tok_details =
        content::Details<TokenService::TokenAvailableDetails>(details).ptr();
    if (tok_details->service() != GaiaConstants::kGaiaOAuth2LoginRefreshToken)
      return;
  }
  // The GaiaConstants::kGaiaOAuth2LoginRefreshToken token is used to create
  // OAuth2 access tokens. If this token either changes or is cleared, any
  // available tokens must be invalidated.
  ClearCache();
  UpdateAuthError(GoogleServiceAuthError::AuthErrorNone());
}

GoogleServiceAuthError ProfileOAuth2TokenService::GetAuthStatus() const {
  return last_auth_error_;
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
