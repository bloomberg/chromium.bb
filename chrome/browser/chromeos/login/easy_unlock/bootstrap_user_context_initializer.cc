// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_context_initializer.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "crypto/random.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

namespace chromeos {

namespace {

const int kMaxGaiaRetries = 5;
const int kUserKeyByteSize = 16;

}  // namespace

BootstrapUserContextInitializer::CompleteCallback*
    BootstrapUserContextInitializer::complete_callback_for_testing_ = NULL;

// static
void BootstrapUserContextInitializer::SetCompleteCallbackForTesting(
    CompleteCallback* callback) {
  complete_callback_for_testing_ = callback;
}

BootstrapUserContextInitializer::BootstrapUserContextInitializer() {
}

BootstrapUserContextInitializer::~BootstrapUserContextInitializer() {
}

void BootstrapUserContextInitializer::Start(const std::string& auth_code,
                                            const CompleteCallback& callback) {
  DCHECK(!callback.is_null());
  callback_ = callback;

  InitializeUserContext();
  StartTokenFetch(auth_code);
}

void BootstrapUserContextInitializer::StartTokenFetch(
    const std::string& auth_code) {
  DCHECK(!token_fetcher_);

  gaia::OAuthClientInfo client_info;
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  client_info.client_id = gaia_urls->oauth2_chrome_client_id();
  client_info.client_secret = gaia_urls->oauth2_chrome_client_secret();

  token_fetcher_.reset(
      new gaia::GaiaOAuthClient(g_browser_process->system_request_context()));
  token_fetcher_->GetTokensFromAuthCode(client_info, auth_code, kMaxGaiaRetries,
                                        this);
}

void BootstrapUserContextInitializer::InitializeUserContext() {
  user_context_.SetAuthFlow(UserContext::AUTH_FLOW_EASY_BOOTSTRAP);

  std::string random_initial_key;
  crypto::RandBytes(WriteInto(&random_initial_key, kUserKeyByteSize + 1),
                    kUserKeyByteSize);

  user_context_.SetKey(Key(random_initial_key));
}

void BootstrapUserContextInitializer::Notify(bool success) {
  if (complete_callback_for_testing_)
    complete_callback_for_testing_->Run(success, user_context_);

  if (callback_.is_null())
    return;

  callback_.Run(success, user_context_);
}

void BootstrapUserContextInitializer::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  user_context_.SetRefreshToken(refresh_token);

  gaia::OAuthClientInfo client_info;
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  client_info.client_id = gaia_urls->oauth2_chrome_client_id();
  client_info.client_secret = gaia_urls->oauth2_chrome_client_secret();

  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kGoogleUserInfoEmail);
  scopes.push_back(GaiaConstants::kGoogleUserInfoProfile);

  token_fetcher_->RefreshToken(client_info, refresh_token, scopes,
                               kMaxGaiaRetries, this);
}

void BootstrapUserContextInitializer::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  token_fetcher_->GetUserInfo(access_token, kMaxGaiaRetries, this);
}

void BootstrapUserContextInitializer::OnGetUserInfoResponse(
    scoped_ptr<base::DictionaryValue> user_info) {
  std::string email;
  std::string gaia_id;
  if (!user_info->GetString("email", &email) ||
      !user_info->GetString("id", &gaia_id)) {
    LOG(ERROR) << "Bad user info.";
    Notify(false);
    return;
  }

  user_context_.SetUserID(email);
  Notify(true);
}

void BootstrapUserContextInitializer::OnOAuthError() {
  LOG(ERROR) << "Auth error.";
  Notify(false);
}

void BootstrapUserContextInitializer::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error.";
  Notify(false);
}

}  // namespace chromeos
