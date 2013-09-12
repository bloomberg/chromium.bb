// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"

FakeProfileOAuth2TokenService::PendingRequest::PendingRequest() {
}

FakeProfileOAuth2TokenService::PendingRequest::~PendingRequest() {
}

// static
BrowserContextKeyedService* FakeProfileOAuth2TokenService::Build(
    content::BrowserContext* profile) {
  FakeProfileOAuth2TokenService* service = new FakeProfileOAuth2TokenService();
  service->Initialize(reinterpret_cast<Profile*>(profile));
  return service;
}

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService() {
}

FakeProfileOAuth2TokenService::~FakeProfileOAuth2TokenService() {
}

void FakeProfileOAuth2TokenService::IssueRefreshToken(
    const std::string& token) {
  refresh_token_ = token;
  if (refresh_token_.empty())
    FireRefreshTokenRevoked("account_id");
  else
    FireRefreshTokenAvailable("account_id");
  // TODO(atwilson): Maybe we should also call FireRefreshTokensLoaded() here?
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  CompleteRequests(false,
                   scope,
                   GoogleServiceAuthError::AuthErrorNone(),
                   access_token,
                   expiration);
}

void FakeProfileOAuth2TokenService::IssueErrorForScope(
    const ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  CompleteRequests(false, scope, error, std::string(), base::Time());
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  CompleteRequests(true, ScopeSet(), error, std::string(), base::Time());
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  CompleteRequests(true,
                   ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   access_token,
                   expiration);
}

void FakeProfileOAuth2TokenService::CompleteRequests(
    bool all_scopes,
    const ScopeSet& scope,
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration) {
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests =
      GetPendingRequests();
  // Walk the requests and notify the callbacks.
  for (std::vector<PendingRequest>::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    if (it->request && (all_scopes || it->scopes == scope))
      it->request->InformConsumer(error, access_token, expiration);
  }
}

std::string FakeProfileOAuth2TokenService::GetRefreshToken() {
  return refresh_token_;
}

net::URLRequestContextGetter*
FakeProfileOAuth2TokenService::GetRequestContext() {
  return NULL;
}

std::vector<FakeProfileOAuth2TokenService::PendingRequest>
FakeProfileOAuth2TokenService::GetPendingRequests() {
  std::vector<PendingRequest> valid_requests;
  for (std::vector<PendingRequest>::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    if (it->request)
      valid_requests.push_back(*it);
  }
  return valid_requests;
}

void FakeProfileOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);
}
