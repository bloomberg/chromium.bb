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
#include "google_apis/gaia/google_service_auth_error.h"

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace chromeos {

class DeviceOAuth2TokenService;

class DeviceOAuth2TokenServiceDelegate {
 public:
  DeviceOAuth2TokenServiceDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceOAuth2TokenService* service);
  ~DeviceOAuth2TokenServiceDelegate();

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() const;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer);

  class ValidationStatusDelegate {
   public:
    virtual void OnValidationCompleted(GoogleServiceAuthError::State error) {}
  };

 private:
  friend class DeviceOAuth2TokenService;
  friend class DeviceOAuth2TokenServiceTest;

  // Dependencies.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // TODO(https://crbug.com/967598): Completely merge this class into
  // DeviceOAuth2TokenService.
  DeviceOAuth2TokenService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenServiceDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
