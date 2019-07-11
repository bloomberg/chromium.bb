// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_delegate.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

DeviceOAuth2TokenServiceDelegate::DeviceOAuth2TokenServiceDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceOAuth2TokenService* service)
    : url_loader_factory_(url_loader_factory),
      state_(STATE_LOADING),
      max_refresh_token_validation_retries_(3),
      validation_requested_(false),
      validation_status_delegate_(nullptr),
      service_(service) {}

DeviceOAuth2TokenServiceDelegate::~DeviceOAuth2TokenServiceDelegate() = default;

void DeviceOAuth2TokenServiceDelegate::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  gaia_oauth_client_->GetTokenInfo(access_token,
                                   max_refresh_token_validation_retries_, this);
}

void DeviceOAuth2TokenServiceDelegate::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  std::string gaia_robot_id;
  // For robot accounts email id is the account id.
  token_info->GetString("email", &gaia_robot_id);
  gaia_oauth_client_.reset();

  service_->CheckRobotAccountId(CoreAccountId(gaia_robot_id));
}

void DeviceOAuth2TokenServiceDelegate::OnOAuthError() {
  gaia_oauth_client_.reset();
  state_ = STATE_TOKEN_INVALID;
  ReportServiceError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
}

void DeviceOAuth2TokenServiceDelegate::OnNetworkError(int response_code) {
  gaia_oauth_client_.reset();

  // Go back to pending validation state. That'll allow a retry on subsequent
  // token minting requests.
  state_ = STATE_VALIDATION_PENDING;
  ReportServiceError(GoogleServiceAuthError::CONNECTION_FAILED);
}

scoped_refptr<network::SharedURLLoaderFactory>
DeviceOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return url_loader_factory_;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
DeviceOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  std::string refresh_token = service_->GetRefreshToken();
  DCHECK(!refresh_token.empty());
  return std::make_unique<OAuth2AccessTokenFetcherImpl>(
      consumer, url_loader_factory, refresh_token);
}

void DeviceOAuth2TokenServiceDelegate::StartValidation() {
  DCHECK_EQ(state_, STATE_VALIDATION_PENDING);
  DCHECK(!gaia_oauth_client_);

  state_ = STATE_VALIDATION_STARTED;

  gaia_oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory_);

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  gaia::OAuthClientInfo client_info;
  client_info.client_id = gaia_urls->oauth2_chrome_client_id();
  client_info.client_secret = gaia_urls->oauth2_chrome_client_secret();

  gaia_oauth_client_->RefreshToken(
      client_info, service_->refresh_token_,
      std::vector<std::string>(1, GaiaConstants::kOAuthWrapBridgeUserInfoScope),
      max_refresh_token_validation_retries_, this);
}

void DeviceOAuth2TokenServiceDelegate::RequestValidation() {
  validation_requested_ = true;
}

void DeviceOAuth2TokenServiceDelegate::InitializeWithValidationStatusDelegate(
    ValidationStatusDelegate* delegate) {
  validation_status_delegate_ = delegate;
}

void DeviceOAuth2TokenServiceDelegate::ClearValidationStatusDelegate() {
  validation_status_delegate_ = nullptr;
}

void DeviceOAuth2TokenServiceDelegate::ReportServiceError(
    GoogleServiceAuthError::State error) {
  if (validation_status_delegate_) {
    validation_status_delegate_->OnValidationCompleted(error);
  }
}

}  // namespace chromeos
