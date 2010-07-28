// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/gaia/token_service.h"

#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"

void TokenService::Initialize(
    const char* const source,
    URLRequestContextGetter* getter,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {

  credentials_ = credentials;
  source_ = std::string(source);
  sync_token_fetcher_.reset(new GaiaAuthenticator2(this, source_, getter));
  talk_token_fetcher_.reset(new GaiaAuthenticator2(this, source_, getter));
}

const bool TokenService::AreCredentialsValid() const {
  return !credentials_.lsid.empty() && !credentials_.sid.empty();
}

const bool TokenService::HasLsid() const {
  return !credentials_.lsid.empty();
}

const std::string& TokenService::GetLsid() const {
  return credentials_.lsid;
}

void TokenService::StartFetchingTokens() {
  DCHECK(AreCredentialsValid());
  sync_token_fetcher_->StartIssueAuthToken(credentials_.sid,
                                           credentials_.lsid,
                                           GaiaConstants::kSyncService);
  talk_token_fetcher_->StartIssueAuthToken(credentials_.sid,
                                           credentials_.lsid,
                                           GaiaConstants::kTalkService);
}

// Services dependent on a token will check if a token is available.
// If it isn't, they'll go to sleep until they get a token event.
const bool TokenService::HasTokenForService(const char* const service) const {
  return token_map_.count(service) > 0;
}

const std::string& TokenService::GetTokenForService(
    const char* const service) const {

  if (token_map_.count(service) > 0) {
    // map[key] is not const
    return (*token_map_.find(service)).second;
  }
  return EmptyString();
}

void TokenService::OnIssueAuthTokenSuccess(const std::string& service,
                                           const std::string& auth_token) {
  LOG(INFO) << "Got an authorization token for " << service;
  token_map_[service] = auth_token;
  TokenAvailableDetails details(service, auth_token);
  NotificationService::current()->Notify(
      NotificationType::TOKEN_AVAILABLE,
      Source<TokenService>(this),
      Details<const TokenAvailableDetails>(&details));
}
void TokenService::OnIssueAuthTokenFailure(const std::string& service,
                                           const GaiaAuthError& error) {
  LOG(WARNING) << "Auth token issuing failed for service:" << service;
  TokenRequestFailedDetails details(service, error);
  NotificationService::current()->Notify(
      NotificationType::TOKEN_REQUEST_FAILED,
      Source<TokenService>(this),
      Details<const TokenRequestFailedDetails>(&details));
}
