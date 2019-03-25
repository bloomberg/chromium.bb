// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/util/tpm_util.h"

#include <stdint.h>

#include "base/logging.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

namespace chromeos {
namespace tpm_util {

namespace {

constexpr char kAttrMode[] = "enterprise.mode";
constexpr char kDeviceModeEnterpriseAD[] = "enterprise_ad";

}  // namespace

bool TpmIsEnabled() {
  bool result = false;
  CryptohomeClient::Get()->CallTpmIsEnabledAndBlock(&result);
  return result;
}

bool TpmIsOwned() {
  bool result = false;
  CryptohomeClient::Get()->CallTpmIsOwnedAndBlock(&result);
  return result;
}

bool TpmIsBeingOwned() {
  bool result = false;
  CryptohomeClient::Get()->CallTpmIsBeingOwnedAndBlock(&result);
  return result;
}

bool InstallAttributesGet(const std::string& name, std::string* value) {
  std::vector<uint8_t> buf;
  bool success = false;
  CryptohomeClient::Get()->InstallAttributesGet(name, &buf, &success);
  if (success) {
    // Cryptohome returns 'buf' with a terminating '\0' character.
    DCHECK(!buf.empty());
    DCHECK_EQ(buf.back(), 0);
    value->assign(reinterpret_cast<char*>(buf.data()), buf.size() - 1);
  }
  return success;
}

bool InstallAttributesSet(const std::string& name, const std::string& value) {
  std::vector<uint8_t> buf(value.c_str(), value.c_str() + value.size() + 1);
  bool success = false;
  CryptohomeClient::Get()->InstallAttributesSet(name, buf, &success);
  return success;
}

bool InstallAttributesFinalize() {
  bool success = false;
  CryptohomeClient::Get()->InstallAttributesFinalize(&success);
  return success;
}

bool InstallAttributesIsInvalid() {
  bool result = false;
  CryptohomeClient::Get()->InstallAttributesIsInvalid(&result);
  return result;
}

bool InstallAttributesIsFirstInstall() {
  bool result = false;
  CryptohomeClient::Get()->InstallAttributesIsFirstInstall(&result);
  return result;
}

bool IsActiveDirectoryLocked() {
  std::string mode;
  return InstallAttributesGet(kAttrMode, &mode) &&
         mode == kDeviceModeEnterpriseAD;
}

bool LockDeviceActiveDirectoryForTesting(const std::string& realm) {
  return InstallAttributesSet("enterprise.owned", "true") &&
         InstallAttributesSet(kAttrMode, kDeviceModeEnterpriseAD) &&
         InstallAttributesSet("enterprise.realm", realm) &&
         InstallAttributesFinalize();
}

}  // namespace tpm_util
}  // namespace chromeos
