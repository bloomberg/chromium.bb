// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_service.h"

#include "chromeos/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"

namespace cryptauth {

// static
void CryptAuthService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  chromeos::device_sync::CryptAuthGCMManager::RegisterPrefs(registry);
  chromeos::device_sync::CryptAuthDeviceManager::RegisterPrefs(registry);
  chromeos::device_sync::CryptAuthEnrollmentManager::RegisterPrefs(registry);
}

}  // namespace cryptauth
