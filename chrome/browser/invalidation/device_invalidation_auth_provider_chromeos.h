// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_DEVICE_INVALIDATION_AUTH_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_INVALIDATION_DEVICE_INVALIDATION_AUTH_PROVIDER_CHROMEOS_H_

#include "base/macros.h"
#include "chrome/browser/invalidation/invalidation_auth_provider.h"

namespace chromeos {
class DeviceOAuth2TokenService;
}

namespace invalidation {

// Authentication provider implementation backed by DeviceOAuth2TokenService.
class DeviceInvalidationAuthProvider : public InvalidationAuthProvider {
 public:
  DeviceInvalidationAuthProvider(
      chromeos::DeviceOAuth2TokenService* token_service);
  virtual ~DeviceInvalidationAuthProvider();

  // InvalidationAuthProvider:
  virtual std::string GetAccountId() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;
  virtual bool ShowLoginUI() OVERRIDE;

 private:
  chromeos::DeviceOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInvalidationAuthProvider);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_DEVICE_INVALIDATION_AUTH_PROVIDER_CHROMEOS_H_
