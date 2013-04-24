// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/cryptohome_library.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

DeviceOAuth2TokenService::DeviceOAuth2TokenService(
    net::URLRequestContextGetter* getter,
    PrefService* local_state)
    : OAuth2TokenService(getter),
      local_state_(local_state) {
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
}

// static
void DeviceOAuth2TokenService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshToken,
                               std::string());
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string encrypted_refresh_token =
      CryptohomeLibrary::Get()->EncryptWithSystemSalt(refresh_token);

  local_state_->SetString(prefs::kDeviceRobotAnyApiRefreshToken,
                          encrypted_refresh_token);
}

std::string DeviceOAuth2TokenService::GetRefreshToken() {
  if (refresh_token_.empty()) {
    std::string encrypted_refresh_token =
        local_state_->GetString(prefs::kDeviceRobotAnyApiRefreshToken);

    refresh_token_ = CryptohomeLibrary::Get()->DecryptWithSystemSalt(
        encrypted_refresh_token);
  }
  return refresh_token_;
}

}  // namespace chromeos
