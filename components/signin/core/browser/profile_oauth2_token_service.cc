// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "net/url_request/url_request_context_getter.h"

ProfileOAuth2TokenService::ProfileOAuth2TokenService()
    : client_(NULL) {}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  DCHECK(!signin_error_controller_.get()) <<
      "ProfileOAuth2TokenService::Initialize called but not "
      "ProfileOAuth2TokenService::Shutdown";
}

void ProfileOAuth2TokenService::Initialize(SigninClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;

  signin_error_controller_.reset(new SigninErrorController());
}

void ProfileOAuth2TokenService::Shutdown() {
  DCHECK(client_) << "Shutdown() called without matching call to Initialize()";
  signin_error_controller_.reset();
}

net::URLRequestContextGetter* ProfileOAuth2TokenService::GetRequestContext() {
  return NULL;
}

void ProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  NOTREACHED();
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

