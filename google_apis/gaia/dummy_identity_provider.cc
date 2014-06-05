// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/dummy_identity_provider.h"

#include <string>

#include "google_apis/gaia/oauth2_token_service.h"

class DummyIdentityProvider::DummyOAuth2TokenService
    : public OAuth2TokenService {
 public:
  DummyOAuth2TokenService();
  virtual ~DummyOAuth2TokenService();

  // OAuth2TokenService:
  virtual bool RefreshTokenIsAvailable(
      const std::string& account_id) const OVERRIDE;
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;
  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) OVERRIDE;

 private:
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DummyOAuth2TokenService);
};


DummyIdentityProvider::DummyOAuth2TokenService::DummyOAuth2TokenService() {
}

DummyIdentityProvider::DummyOAuth2TokenService::~DummyOAuth2TokenService() {
}

bool DummyIdentityProvider::DummyOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return false;
}

void DummyIdentityProvider::DummyOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  // Ignore the fetch request.
}

OAuth2AccessTokenFetcher*
DummyIdentityProvider::DummyOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  return NULL;
}

net::URLRequestContextGetter*
DummyIdentityProvider::DummyOAuth2TokenService::GetRequestContext() {
  return NULL;
}

DummyIdentityProvider::DummyIdentityProvider()
    : oauth2_token_service_(new DummyOAuth2TokenService) {
}

DummyIdentityProvider::~DummyIdentityProvider() {
}

std::string DummyIdentityProvider::GetActiveUsername() {
  return std::string();
}

std::string DummyIdentityProvider::GetActiveAccountId() {
  return std::string();
}

OAuth2TokenService* DummyIdentityProvider::GetTokenService() {
  return oauth2_token_service_.get();
}

bool DummyIdentityProvider::RequestLogin() {
  return false;
}
