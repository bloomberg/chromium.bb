// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace gaia {
class GaiaOAuthClient;
}

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace chromeos {

class DeviceOAuth2TokenService;

class DeviceOAuth2TokenServiceDelegate
    : public gaia::GaiaOAuthClient::Delegate {
 public:
  DeviceOAuth2TokenServiceDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceOAuth2TokenService* service);
  ~DeviceOAuth2TokenServiceDelegate() override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() const;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer);

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  class ValidationStatusDelegate {
   public:
    virtual void OnValidationCompleted(GoogleServiceAuthError::State error) {}
  };

 private:
  friend class DeviceOAuth2TokenService;
  friend class DeviceOAuth2TokenServiceTest;

  // Describes the operational state of this object.
  enum State {
    // Pending system salt / refresh token load.
    STATE_LOADING,
    // No token available.
    STATE_NO_TOKEN,
    // System salt loaded, validation not started yet.
    STATE_VALIDATION_PENDING,
    // Refresh token validation underway.
    STATE_VALIDATION_STARTED,
    // Token validation failed.
    STATE_TOKEN_INVALID,
    // Refresh token is valid.
    STATE_TOKEN_VALID,
  };

  // Starts the token validation flow, i.e. token info fetch.
  void StartValidation();

  void RequestValidation();

  void InitializeWithValidationStatusDelegate(
      ValidationStatusDelegate* delegate);
  void ClearValidationStatusDelegate();

  void ReportServiceError(GoogleServiceAuthError::State error);

  // Dependencies.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Current operational state.
  State state_;

  int max_refresh_token_validation_retries_;

  // Flag to indicate whether there are pending requests.
  bool validation_requested_;

  // Validation status delegate
  ValidationStatusDelegate* validation_status_delegate_;

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  // TODO(https://crbug.com/967598): Completely merge this class into
  // DeviceOAuth2TokenService.
  DeviceOAuth2TokenService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenServiceDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
