// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service.h"

FakeOAuth2TokenService::FakeOAuth2TokenService() {
}

FakeOAuth2TokenService::~FakeOAuth2TokenService() {
}

std::string FakeOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) const {
  return std::string();
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
  return NULL;
}
