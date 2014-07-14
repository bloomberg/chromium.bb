// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "policy/proto/device_management_backend.pb.h"

namespace chromeos {

struct DeviceOAuth2TokenService::PendingRequest {
  PendingRequest(const base::WeakPtr<RequestImpl>& request,
                 const std::string& client_id,
                 const std::string& client_secret,
                 const ScopeSet& scopes)
      : request(request),
        client_id(client_id),
        client_secret(client_secret),
        scopes(scopes) {}

  const base::WeakPtr<RequestImpl> request;
  const std::string client_id;
  const std::string client_secret;
  const ScopeSet scopes;
};

DeviceOAuth2TokenService::DeviceOAuth2TokenService(
    net::URLRequestContextGetter* getter,
    PrefService* local_state)
    : url_request_context_getter_(getter),
      local_state_(local_state),
      state_(STATE_LOADING),
      max_refresh_token_validation_retries_(3),
      weak_ptr_factory_(this) {
  // Pull in the system salt.
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&DeviceOAuth2TokenService::DidGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr()));
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);
  FlushTokenSaveCallbacks(false);
}

// static
void DeviceOAuth2TokenService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshToken,
                               std::string());
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    const StatusCallback& result_callback) {
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);

  bool waiting_for_salt = state_ == STATE_LOADING;
  refresh_token_ = refresh_token;
  state_ = STATE_VALIDATION_PENDING;
  FireRefreshTokenAvailable(GetRobotAccountId());

  token_save_callbacks_.push_back(result_callback);
  if (!waiting_for_salt) {
    if (system_salt_.empty())
      FlushTokenSaveCallbacks(false);
    else
      EncryptAndSaveToken();
  }
}

bool DeviceOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  switch (state_) {
    case STATE_NO_TOKEN:
    case STATE_TOKEN_INVALID:
      return false;
    case STATE_LOADING:
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      return account_id == GetRobotAccountId();
  }

  NOTREACHED() << "Unhandled state " << state_;
  return false;
}

std::string DeviceOAuth2TokenService::GetRobotAccountId() const {
  std::string result;
  CrosSettings::Get()->GetString(kServiceAccountIdentity, &result);
  return result;
}

void DeviceOAuth2TokenService::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  gaia_oauth_client_->GetTokenInfo(
      access_token,
      max_refresh_token_validation_retries_,
      this);
}

void DeviceOAuth2TokenService::OnGetTokenInfoResponse(
    scoped_ptr<base::DictionaryValue> token_info) {
  std::string gaia_robot_id;
  token_info->GetString("email", &gaia_robot_id);
  gaia_oauth_client_.reset();

  CheckRobotAccountId(gaia_robot_id);
}

void DeviceOAuth2TokenService::OnOAuthError() {
  gaia_oauth_client_.reset();
  state_ = STATE_TOKEN_INVALID;
  FlushPendingRequests(false, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
}

void DeviceOAuth2TokenService::OnNetworkError(int response_code) {
  gaia_oauth_client_.reset();

  // Go back to pending validation state. That'll allow a retry on subsequent
  // token minting requests.
  state_ = STATE_VALIDATION_PENDING;
  FlushPendingRequests(false, GoogleServiceAuthError::CONNECTION_FAILED);
}

std::string DeviceOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) const {
  switch (state_) {
    case STATE_LOADING:
    case STATE_NO_TOKEN:
    case STATE_TOKEN_INVALID:
      // This shouldn't happen: GetRefreshToken() is only called for actual
      // token minting operations. In above states, requests are either queued
      // or short-circuited to signal error immediately, so no actual token
      // minting via OAuth2TokenService::FetchOAuth2Token should be triggered.
      NOTREACHED();
      return std::string();
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      return refresh_token_;
  }

  NOTREACHED() << "Unhandled state " << state_;
  return std::string();
}

net::URLRequestContextGetter* DeviceOAuth2TokenService::GetRequestContext() {
  return url_request_context_getter_.get();
}

void DeviceOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  switch (state_) {
    case STATE_VALIDATION_PENDING:
      // If this is the first request for a token, start validation.
      StartValidation();
      // fall through.
    case STATE_LOADING:
    case STATE_VALIDATION_STARTED:
      // Add a pending request that will be satisfied once validation completes.
      pending_requests_.push_back(new PendingRequest(
          request->AsWeakPtr(), client_id, client_secret, scopes));
      return;
    case STATE_NO_TOKEN:
      FailRequest(request, GoogleServiceAuthError::USER_NOT_SIGNED_UP);
      return;
    case STATE_TOKEN_INVALID:
      FailRequest(request, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      return;
    case STATE_TOKEN_VALID:
      // Pass through to OAuth2TokenService to satisfy the request.
      OAuth2TokenService::FetchOAuth2Token(
          request, account_id, getter, client_id, client_secret, scopes);
      return;
  }

  NOTREACHED() << "Unexpected state " << state_;
}

OAuth2AccessTokenFetcher* DeviceOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  std::string refresh_token = GetRefreshToken(account_id);
  DCHECK(!refresh_token.empty());
  return new OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token);
}


void DeviceOAuth2TokenService::DidGetSystemSalt(
    const std::string& system_salt) {
  system_salt_ = system_salt;

  // Bail out if system salt is not available.
  if (system_salt_.empty()) {
    LOG(ERROR) << "Failed to get system salt.";
    FlushTokenSaveCallbacks(false);
    state_ = STATE_NO_TOKEN;
    FireRefreshTokensLoaded();
    return;
  }

  // If the token has been set meanwhile, write it to |local_state_|.
  if (!refresh_token_.empty()) {
    EncryptAndSaveToken();
    FireRefreshTokensLoaded();
    return;
  }

  // Otherwise, load the refresh token from |local_state_|.
  std::string encrypted_refresh_token =
      local_state_->GetString(prefs::kDeviceRobotAnyApiRefreshToken);
  if (!encrypted_refresh_token.empty()) {
    CryptohomeTokenEncryptor encryptor(system_salt_);
    refresh_token_ = encryptor.DecryptWithSystemSalt(encrypted_refresh_token);
    if (refresh_token_.empty()) {
      LOG(ERROR) << "Failed to decrypt refresh token.";
      state_ = STATE_NO_TOKEN;
      FireRefreshTokensLoaded();
      return;
    }
  }

  state_ = STATE_VALIDATION_PENDING;

  // If there are pending requests, start a validation.
  if (!pending_requests_.empty())
    StartValidation();

  // Announce the token.
  FireRefreshTokenAvailable(GetRobotAccountId());
  FireRefreshTokensLoaded();
}

void DeviceOAuth2TokenService::CheckRobotAccountId(
    const std::string& gaia_robot_id) {
  // Make sure the value returned by GetRobotAccountId has been validated
  // against current device settings.
  switch (CrosSettings::Get()->PrepareTrustedValues(
      base::Bind(&DeviceOAuth2TokenService::CheckRobotAccountId,
                 weak_ptr_factory_.GetWeakPtr(),
                 gaia_robot_id))) {
    case CrosSettingsProvider::TRUSTED:
      // All good, compare account ids below.
      break;
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // The callback passed to PrepareTrustedValues above will trigger a
      // re-check eventually.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // There's no trusted account id, which is equivalent to no token present.
      LOG(WARNING) << "Device settings permanently untrusted.";
      state_ = STATE_NO_TOKEN;
      FlushPendingRequests(false, GoogleServiceAuthError::USER_NOT_SIGNED_UP);
      return;
  }

  std::string policy_robot_id = GetRobotAccountId();
  if (policy_robot_id == gaia_robot_id) {
    state_ = STATE_TOKEN_VALID;
    FlushPendingRequests(true, GoogleServiceAuthError::NONE);
  } else {
    if (gaia_robot_id.empty()) {
      LOG(WARNING) << "Device service account owner in policy is empty.";
    } else {
      LOG(WARNING) << "Device service account owner in policy does not match "
                   << "refresh token owner \"" << gaia_robot_id << "\".";
    }
    state_ = STATE_TOKEN_INVALID;
    FlushPendingRequests(false,
                         GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  }
}

void DeviceOAuth2TokenService::EncryptAndSaveToken() {
  DCHECK_NE(state_, STATE_LOADING);

  CryptohomeTokenEncryptor encryptor(system_salt_);
  std::string encrypted_refresh_token =
      encryptor.EncryptWithSystemSalt(refresh_token_);
  bool result = true;
  if (encrypted_refresh_token.empty()) {
    LOG(ERROR) << "Failed to encrypt refresh token; save aborted.";
    result = false;
  } else {
    local_state_->SetString(prefs::kDeviceRobotAnyApiRefreshToken,
                            encrypted_refresh_token);
  }

  FlushTokenSaveCallbacks(result);
}

void DeviceOAuth2TokenService::StartValidation() {
  DCHECK_EQ(state_, STATE_VALIDATION_PENDING);
  DCHECK(!gaia_oauth_client_);

  state_ = STATE_VALIDATION_STARTED;

  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(
      g_browser_process->system_request_context()));

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  gaia::OAuthClientInfo client_info;
  client_info.client_id = gaia_urls->oauth2_chrome_client_id();
  client_info.client_secret = gaia_urls->oauth2_chrome_client_secret();

  gaia_oauth_client_->RefreshToken(
      client_info,
      refresh_token_,
      std::vector<std::string>(1, GaiaConstants::kOAuthWrapBridgeUserInfoScope),
      max_refresh_token_validation_retries_,
      this);
}

void DeviceOAuth2TokenService::FlushPendingRequests(
    bool token_is_valid,
    GoogleServiceAuthError::State error) {
  std::vector<PendingRequest*> requests;
  requests.swap(pending_requests_);
  for (std::vector<PendingRequest*>::iterator request(requests.begin());
       request != requests.end();
       ++request) {
    scoped_ptr<PendingRequest> scoped_request(*request);
    if (!scoped_request->request)
      continue;

    if (token_is_valid) {
      OAuth2TokenService::FetchOAuth2Token(
          scoped_request->request.get(),
          scoped_request->request->GetAccountId(),
          GetRequestContext(),
          scoped_request->client_id,
          scoped_request->client_secret,
          scoped_request->scopes);
    } else {
      FailRequest(scoped_request->request.get(), error);
    }
  }
}

void DeviceOAuth2TokenService::FlushTokenSaveCallbacks(bool result) {
  std::vector<StatusCallback> callbacks;
  callbacks.swap(token_save_callbacks_);
  for (std::vector<StatusCallback>::iterator callback(callbacks.begin());
       callback != callbacks.end();
       ++callback) {
    if (!callback->is_null())
      callback->Run(result);
  }
}

void DeviceOAuth2TokenService::FailRequest(
    RequestImpl* request,
    GoogleServiceAuthError::State error) {
  GoogleServiceAuthError auth_error(error);
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &RequestImpl::InformConsumer,
      request->AsWeakPtr(),
      auth_error,
      std::string(),
      base::Time()));
}

}  // namespace chromeos
