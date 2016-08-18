// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/identity_source.h"

#include "base/memory/ptr_util.h"

namespace blimp {
namespace client {

namespace {
// OAuth2 token scope.
const char kOAuth2TokenScope[] =
    "https://www.googleapis.com/auth/userinfo.email";
}  // namespace

IdentitySource::IdentitySource(BlimpClientContextDelegate* delegate,
                               const TokenCallback& callback)
    : OAuth2TokenService::Consumer("blimp_client"),
      token_callback_(callback),
      is_fetching_token_(false),
      delegate_(delegate) {
  DCHECK(delegate_);

  // Create identity provider.
  identity_provider_ = delegate_->CreateIdentityProvider();
  DCHECK(identity_provider_.get());
  identity_provider_->AddObserver(this);
}

IdentitySource::~IdentitySource() {
  identity_provider_->RemoveActiveAccountRefreshTokenObserver(this);
  identity_provider_->RemoveObserver(this);
}

void IdentitySource::Connect() {
  if (is_fetching_token_) {
    return;
  }

  const std::string& account_id = identity_provider_->GetActiveAccountId();

  // User must sign in first.
  if (account_id.empty()) {
    delegate_->OnAuthenticationError(
        BlimpClientContextDelegate::AuthError::NOT_SIGNED_IN);
    return;
  }

  account_id_ = account_id;
  is_fetching_token_ = true;
  FetchAuthToken();
}

void IdentitySource::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  token_request_.reset();
  is_fetching_token_ = false;

  if (token_callback_) {
    token_callback_.Run(access_token);
  }
}

// Fail to get the token after retries attempts in native layer and Java layer.
void IdentitySource::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  token_request_.reset();
  is_fetching_token_ = false;

  DCHECK(delegate_);
  delegate_->OnAuthenticationError(
      BlimpClientContextDelegate::AuthError::OAUTH_TOKEN_FAIL);
}

void IdentitySource::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != account_id_) {
    return;
  }

  identity_provider_->RemoveActiveAccountRefreshTokenObserver(this);
  FetchAuthToken();
}

void IdentitySource::FetchAuthToken() {
  OAuth2TokenService* token_service = identity_provider_->GetTokenService();
  DCHECK(token_service);

  if (token_service->RefreshTokenIsAvailable(account_id_)) {
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(kOAuth2TokenScope);
    token_request_ = token_service->StartRequest(account_id_, scopes, this);
  } else {
    identity_provider_->AddActiveAccountRefreshTokenObserver(this);
  }
}

}  // namespace client
}  // namespace blimp
