// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_context_initializer.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
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

BootstrapUserContextInitializer::BootstrapUserContextInitializer()
    : random_key_used_(false),
      weak_ptr_factory_(this) {
}

BootstrapUserContextInitializer::~BootstrapUserContextInitializer() {
}

void BootstrapUserContextInitializer::Start(const std::string& auth_code,
                                            const CompleteCallback& callback) {
  DCHECK(!callback.is_null());
  callback_ = callback;

  user_context_.SetAuthFlow(UserContext::AUTH_FLOW_EASY_BOOTSTRAP);
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

void BootstrapUserContextInitializer::StartCheckExistingKeys() {
  // Use random key for the first time user.
  if (!user_manager::UserManager::Get()->IsKnownUser(
          user_context_.GetAccountId())) {
    CreateRandomKey();
    return;
  }

  EasyUnlockKeyManager* key_manager =
      UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  key_manager->GetDeviceDataList(
      UserContext(user_context_.GetAccountId()),
      base::Bind(&BootstrapUserContextInitializer::OnGetEasyUnlockData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BootstrapUserContextInitializer::OnGetEasyUnlockData(
    bool success,
    const EasyUnlockDeviceKeyDataList& data_list) {
  // Existing user must have Smart lock keys to use bootstrap flow.
  if (!success || data_list.empty()) {
    LOG(ERROR) << "Unable to get Easy unlock key data.";
    Notify(false);
    return;
  }

  EasyUnlockService* service =
      EasyUnlockService::Get(ProfileHelper::GetSigninProfile());
  service->AddObserver(this);

  static_cast<EasyUnlockServiceSignin*>(service)
      ->SetCurrentUser(user_context_.GetAccountId());
  OnScreenlockStateChanged(service->GetScreenlockState());
}

void BootstrapUserContextInitializer::OnEasyUnlockAuthenticated(
    EasyUnlockAuthAttempt::Type auth_attempt_type,
    bool success,
    const AccountId& account_id,
    const std::string& key_secret,
    const std::string& key_label) {
  DCHECK_EQ(EasyUnlockAuthAttempt::TYPE_SIGNIN, auth_attempt_type);
  if (!success || key_secret.empty()) {
    LOG(ERROR) << "Failed to sign-in using existing Smart lock key.";
    Notify(false);
    return;
  }

  user_context_.SetKey(Key(key_secret));
  user_context_.GetKey()->SetLabel(key_label);
  Notify(true);
}

void BootstrapUserContextInitializer::CreateRandomKey() {
  std::string random_initial_key;
  crypto::RandBytes(base::WriteInto(&random_initial_key, kUserKeyByteSize + 1),
                    kUserKeyByteSize);
  user_context_.SetKey(Key(random_initial_key));
  random_key_used_ = true;
  Notify(true);
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
    std::unique_ptr<base::DictionaryValue> user_info) {
  std::string email;
  std::string gaia_id;
  if (!user_info->GetString("email", &email) ||
      !user_info->GetString("id", &gaia_id)) {
    LOG(ERROR) << "Bad user info.";
    Notify(false);
    return;
  }

  user_context_.SetAccountId(user_manager::known_user::GetAccountId(
      user_manager::CanonicalizeUserID(email), gaia_id, AccountType::GOOGLE));
  StartCheckExistingKeys();
}

void BootstrapUserContextInitializer::OnOAuthError() {
  LOG(ERROR) << "Auth error.";
  Notify(false);
}

void BootstrapUserContextInitializer::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error.";
  Notify(false);
}

void BootstrapUserContextInitializer::OnScreenlockStateChanged(
    proximity_auth::ScreenlockState state) {
  // TODO(xiyuan): Add timeout and hook up with error UI after
  //     http://crbug.com/471067.
  if (state != proximity_auth::ScreenlockState::AUTHENTICATED)
    return;

  EasyUnlockService* service =
      EasyUnlockService::Get(ProfileHelper::GetSigninProfile());
  service->RemoveObserver(this);

  service->AttemptAuth(
      user_context_.GetAccountId(),
      base::Bind(&BootstrapUserContextInitializer::OnEasyUnlockAuthenticated,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace chromeos
