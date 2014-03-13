// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service.h"

FakeOAuth2TokenService::FakeOAuth2TokenService() : request_context_(NULL) {}

FakeOAuth2TokenService::~FakeOAuth2TokenService() {
}

void FakeOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
}

void FakeOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
}

net::URLRequestContextGetter* FakeOAuth2TokenService::GetRequestContext() {
  return request_context_;
}

bool FakeOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return account_ids_.count(account_id) != 0;
};

void FakeOAuth2TokenService::AddAccount(const std::string& account_id) {
  account_ids_.insert(account_id);
}

OAuth2AccessTokenFetcher* FakeOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  // |FakeOAuth2TokenService| overrides |FetchOAuth2Token| and thus
  // |CreateAccessTokenFetcher| should never be called.
  NOTREACHED();
  return NULL;
}
