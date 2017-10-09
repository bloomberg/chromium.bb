// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/oauth2_token_initializer.h"

#include "chrome/browser/browser_process.h"

namespace chromeos {

OAuth2TokenInitializer::OAuth2TokenInitializer() {}

OAuth2TokenInitializer::~OAuth2TokenInitializer() {}

void OAuth2TokenInitializer::Start(const UserContext& user_context,
                                   const FetchOAuth2TokensCallback& callback) {
  DCHECK(!user_context.GetAuthCode().empty());
  callback_ = callback;
  user_context_ = user_context;
  oauth2_token_fetcher_.reset(new OAuth2TokenFetcher(
      this, g_browser_process->system_request_context()));
  if (user_context.GetDeviceId().empty())
    NOTREACHED() << "Device ID is not set";
  oauth2_token_fetcher_->StartExchangeFromAuthCode(user_context.GetAuthCode(),
                                                   user_context.GetDeviceId());
}

void OAuth2TokenInitializer::OnOAuth2TokensAvailable(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  VLOG(1) << "OAuth2 tokens fetched";
  user_context_.SetAuthCode(std::string());
  user_context_.SetRefreshToken(oauth2_tokens.refresh_token);
  user_context_.SetAccessToken(oauth2_tokens.access_token);
  callback_.Run(true, user_context_);
}

void OAuth2TokenInitializer::OnOAuth2TokensFetchFailed() {
  LOG(WARNING) << "OAuth2TokenInitializer - OAuth2 token fetch failed";
  callback_.Run(false, user_context_);
}

}  // namespace chromeos
