// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/local_device_data_provider.h"

#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace tether {

LocalDeviceDataProvider::LocalDeviceDataProvider(
    const cryptauth::CryptAuthDeviceManager* cryptauth_device_manager,
    const cryptauth::CryptAuthEnrollmentManager* cryptauth_enrollment_manager)
    : LocalDeviceDataProvider(
          base::MakeUnique<LocalDeviceDataProviderDelegateImpl>(
              cryptauth_device_manager,
              cryptauth_enrollment_manager)) {}

LocalDeviceDataProvider::LocalDeviceDataProvider(
    std::unique_ptr<LocalDeviceDataProviderDelegate> delegate)
    : delegate_(std::move(delegate)) {}

LocalDeviceDataProvider::~LocalDeviceDataProvider() {}

LocalDeviceDataProvider::LocalDeviceDataProviderDelegateImpl::
    LocalDeviceDataProviderDelegateImpl(
        const cryptauth::CryptAuthDeviceManager* cryptauth_device_manager,
        const cryptauth::CryptAuthEnrollmentManager*
            cryptauth_enrollment_manager)
    : cryptauth_device_manager_(cryptauth_device_manager),
      cryptauth_enrollment_manager_(cryptauth_enrollment_manager) {}

LocalDeviceDataProvider::LocalDeviceDataProviderDelegateImpl::
    ~LocalDeviceDataProviderDelegateImpl() {}

std::string
LocalDeviceDataProvider::LocalDeviceDataProviderDelegateImpl::GetUserPublicKey()
    const {
  return cryptauth_enrollment_manager_->GetUserPublicKey();
}

std::vector<cryptauth::ExternalDeviceInfo>
LocalDeviceDataProvider::LocalDeviceDataProviderDelegateImpl::GetSyncedDevices()
    const {
  return cryptauth_device_manager_->GetSyncedDevices();
}

bool LocalDeviceDataProvider::GetLocalDeviceData(
    std::string* public_key_out,
    std::vector<cryptauth::BeaconSeed>* beacon_seeds_out) const {
  DCHECK(public_key_out || beacon_seeds_out);

  std::string public_key = delegate_->GetUserPublicKey();
  if (public_key.empty()) {
    return false;
  }

  std::vector<cryptauth::ExternalDeviceInfo> synced_devices =
      delegate_->GetSyncedDevices();
  for (const auto& device : synced_devices) {
    if (device.has_public_key() && device.public_key() == public_key &&
        device.beacon_seeds_size() > 0) {
      if (public_key_out) {
        public_key_out->assign(public_key);
      }

      if (beacon_seeds_out) {
        beacon_seeds_out->clear();
        for (int i = 0; i < device.beacon_seeds_size(); i++) {
          beacon_seeds_out->push_back(device.beacon_seeds(i));
        }
      }

      return true;
    }
  }

  return false;
}

}  // namespace tether

}  // namespace cryptauth
