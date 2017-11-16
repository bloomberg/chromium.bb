// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_get_keys_operation.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/signin/core/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

EasyUnlockGetKeysOperation::EasyUnlockGetKeysOperation(
    const UserContext& user_context,
    const GetKeysCallback& callback)
    : user_context_(user_context),
      callback_(callback),
      key_index_(0),
      weak_ptr_factory_(this) {}

EasyUnlockGetKeysOperation::~EasyUnlockGetKeysOperation() {}

void EasyUnlockGetKeysOperation::Start() {
  // Register for asynchronous notification of cryptohome being ready.
  DBusThreadManager::Get()->GetCryptohomeClient()->WaitForServiceToBeAvailable(
      base::Bind(&EasyUnlockGetKeysOperation::OnCryptohomeAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockGetKeysOperation::OnCryptohomeAvailable(bool available) {
  if (!available) {
    PA_LOG(ERROR) << "Failed to wait for cryptohome to become available";
    callback_.Run(false, EasyUnlockDeviceKeyDataList());
    return;
  }

  // Start the asynchronous key fetch.
  // TODO(xiyuan): Use ListKeyEx.
  key_index_ = 0;
  GetKeyData();
}

void EasyUnlockGetKeysOperation::GetKeyData() {
  const cryptohome::Identification id(user_context_.GetAccountId());
  cryptohome::GetKeyDataRequest request;
  request.mutable_key()->mutable_data()->set_label(
      EasyUnlockKeyManager::GetKeyLabel(key_index_));
  cryptohome::HomedirMethods::GetInstance()->GetKeyDataEx(
      id, cryptohome::AuthorizationRequest(), request,
      base::Bind(&EasyUnlockGetKeysOperation::OnGetKeyData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockGetKeysOperation::OnGetKeyData(
    bool success,
    cryptohome::MountError return_code,
    const std::vector<cryptohome::KeyDefinition>& key_definitions) {
  if (!success || key_definitions.empty()) {
    // MOUNT_ERROR_KEY_FAILURE is considered as success. Other error codes are
    // treated as failures.
    if (return_code == cryptohome::MOUNT_ERROR_NONE ||
        return_code == cryptohome::MOUNT_ERROR_KEY_FAILURE) {
      callback_.Run(true, devices_);
      return;
    }

    PA_LOG(ERROR) << "Easy unlock failed to get key data, code=" << return_code;
    callback_.Run(false, EasyUnlockDeviceKeyDataList());
    return;
  }

  DCHECK_EQ(1u, key_definitions.size());

  const std::vector<cryptohome::KeyDefinition::ProviderData>& provider_data =
      key_definitions.front().provider_data;

  EasyUnlockDeviceKeyData device;
  for (size_t i = 0; i < provider_data.size(); ++i) {
    const cryptohome::KeyDefinition::ProviderData& entry = provider_data[i];
    if (entry.name == kEasyUnlockKeyMetaNameBluetoothAddress) {
      if (entry.bytes)
        device.bluetooth_address = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameBluetoothType) {
      if (entry.number) {
        if (*entry.number >=
            static_cast<int64_t>(
                EasyUnlockDeviceKeyData::NUM_BLUETOOTH_TYPES)) {
          PA_LOG(ERROR) << "Invalid Bluetooth type: " << *entry.number;
        } else {
          device.bluetooth_type =
              static_cast<EasyUnlockDeviceKeyData::BluetoothType>(
                  *entry.number);
        }
      } else {
        NOTREACHED();
      }
    } else if (entry.name == kEasyUnlockKeyMetaNamePubKey) {
      if (entry.bytes)
        device.public_key = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNamePsk) {
      if (entry.bytes)
        device.psk = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameChallenge) {
      if (entry.bytes)
        device.challenge = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameWrappedSecret) {
      if (entry.bytes)
        device.wrapped_secret = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameSerializedBeaconSeeds) {
      if (entry.bytes)
        device.serialized_beacon_seeds = *entry.bytes;
      else
        NOTREACHED();
    } else {
      PA_LOG(WARNING) << "Unknown EasyUnlock key data entry, name="
                      << entry.name;
    }
  }
  devices_.push_back(device);

  ++key_index_;
  GetKeyData();
}

}  // namespace chromeos
