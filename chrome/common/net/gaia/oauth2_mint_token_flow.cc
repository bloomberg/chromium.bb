// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

using net::URLRequestContextGetter;

namespace {

OAuth2MintTokenFlow::InterceptorForTests* g_interceptor_for_tests = NULL;

}  // namespace

// static
void OAuth2MintTokenFlow::SetInterceptorForTests(
    OAuth2MintTokenFlow::InterceptorForTests* interceptor) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  CHECK(NULL == g_interceptor_for_tests);  // Only one at a time.
  g_interceptor_for_tests = interceptor;
}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(
    URLRequestContextGetter* context,
    Delegate* delegate)
    : context_(context),
      delegate_(delegate),
      state_(INITIAL) {
}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::Start(
    const std::string& login_refresh_token,
    const std::string& extension_id,
    const std::string& client_id,
    const std::vector<std::string>& scopes) {
  login_refresh_token_ = login_refresh_token;
  extension_id_ = extension_id;
  client_id_ = client_id;
  scopes_ = scopes;

  if (g_interceptor_for_tests) {
    std::string auth_token;
    GoogleServiceAuthError error = GoogleServiceAuthError::None();

    // We use PostTask, instead of calling the delegate directly, because the
    // message loop will run a few times before we notify the delegate in the
    // real implementation.
    if (g_interceptor_for_tests->DoIntercept(this, &auth_token, &error)) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&OAuth2MintTokenFlow::Delegate::OnMintTokenSuccess,
                     base::Unretained(delegate_), auth_token));
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&OAuth2MintTokenFlow::Delegate::OnMintTokenFailure,
                     base::Unretained(delegate_), error));
    }
    return;
  }

  BeginGetLoginAccessToken();
}

void OAuth2MintTokenFlow::OnGetTokenSuccess(
    const std::string& access_token) {
  login_access_token_ = access_token;
  EndGetLoginAccessToken(NULL);
}

void OAuth2MintTokenFlow::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  EndGetLoginAccessToken(&error);
}

void OAuth2MintTokenFlow::BeginGetLoginAccessToken() {
  CHECK_EQ(INITIAL, state_);
  state_ = FETCH_LOGIN_ACCESS_TOKEN_STARTED;

  oauth2_access_token_fetcher_.reset(CreateAccessTokenFetcher());
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      login_refresh_token_,
      std::vector<std::string>());
}

void OAuth2MintTokenFlow::EndGetLoginAccessToken(
    const GoogleServiceAuthError* error) {
  CHECK_EQ(FETCH_LOGIN_ACCESS_TOKEN_STARTED, state_);
  if (!error) {
    state_ = FETCH_LOGIN_ACCESS_TOKEN_DONE;
    BeginMintAccessToken();
  } else {
    state_ = ERROR_STATE;
    ReportFailure(*error);
  }
}

void OAuth2MintTokenFlow::OnMintTokenSuccess(
    const std::string& access_token) {
  app_access_token_ = access_token;
  EndMintAccessToken(NULL);
}
void OAuth2MintTokenFlow::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  EndMintAccessToken(&error);
}

void OAuth2MintTokenFlow::BeginMintAccessToken() {
  CHECK_EQ(FETCH_LOGIN_ACCESS_TOKEN_DONE, state_);
  state_ = MINT_ACCESS_TOKEN_STARTED;

  oauth2_mint_token_fetcher_.reset(CreateMintTokenFetcher());
  oauth2_mint_token_fetcher_->Start(
      login_access_token_,
      client_id_,
      scopes_,
      extension_id_);
}

void OAuth2MintTokenFlow::EndMintAccessToken(
    const GoogleServiceAuthError* error) {
  CHECK_EQ(MINT_ACCESS_TOKEN_STARTED, state_);

  if (!error) {
    state_ = MINT_ACCESS_TOKEN_DONE;
    ReportSuccess();
  } else {
    state_ = ERROR_STATE;
    ReportFailure(*error);
  }
}

void OAuth2MintTokenFlow::ReportSuccess() {
  CHECK_EQ(MINT_ACCESS_TOKEN_DONE, state_);

  if (delegate_) {
    delegate_->OnMintTokenSuccess(app_access_token_);
  }
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  CHECK_EQ(ERROR_STATE, state_);

  if (delegate_) {
    delegate_->OnMintTokenFailure(error);
  }
}

OAuth2AccessTokenFetcher* OAuth2MintTokenFlow::CreateAccessTokenFetcher() {
  return new OAuth2AccessTokenFetcher(this, context_);
}

OAuth2MintTokenFetcher* OAuth2MintTokenFlow::CreateMintTokenFetcher() {
  return new OAuth2MintTokenFetcher(this, context_, "OAuth2MintTokenFlow");
}
