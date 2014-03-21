// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/device_invalidation_auth_provider_chromeos.h"

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

namespace invalidation {

DeviceInvalidationAuthProvider::DeviceInvalidationAuthProvider(
    chromeos::DeviceOAuth2TokenService* token_service)
    : token_service_(token_service) {}

DeviceInvalidationAuthProvider::~DeviceInvalidationAuthProvider() {}

std::string DeviceInvalidationAuthProvider::GetAccountId() {
  return token_service_->GetRobotAccountId();
}

OAuth2TokenService* DeviceInvalidationAuthProvider::GetTokenService() {
  return token_service_;
}

bool DeviceInvalidationAuthProvider::ShowLoginUI() { return false; }

}  // namespace invalidation
