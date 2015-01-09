// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/url_request/url_request_context_getter.h"

ProfileOAuth2TokenService::ProfileOAuth2TokenService()
    : client_(NULL),
      signin_error_controller_(NULL) {}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {}

void ProfileOAuth2TokenService::Initialize(
    SigninClient* client,
    SigninErrorController* signin_error_controller) {
  DCHECK(client);
  DCHECK(!client_);
  DCHECK(signin_error_controller);
  DCHECK(!signin_error_controller_);
  client_ = client;
  signin_error_controller_ = signin_error_controller;
}

void ProfileOAuth2TokenService::Shutdown() {
  DCHECK(client_) << "Shutdown() called without matching call to Initialize()";
}

net::URLRequestContextGetter* ProfileOAuth2TokenService::GetRequestContext() {
  return NULL;
}

void ProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  NOTREACHED();
}

void ProfileOAuth2TokenService::ValidateAccountId(
    const std::string& account_id) const {
  DCHECK(!account_id.empty());

  // If the account is given as an email, make sure its a canonical email.
  // Note that some tests don't use email strings as account id, and after
  // the gaia id migration it won't be an email.  So only check for
  // canonicalization if the account_id is suspected to be an email.
  if (account_id.find('@') != std::string::npos)
    DCHECK_EQ(gaia::CanonicalizeEmail(account_id), account_id);
}

std::vector<std::string> ProfileOAuth2TokenService::GetAccounts() {
  NOTREACHED();
  return std::vector<std::string>();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  // Empty implementation by default.
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  NOTREACHED();
}

void ProfileOAuth2TokenService::RevokeAllCredentials() {
  // Empty implementation by default.
}
