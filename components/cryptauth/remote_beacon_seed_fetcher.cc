// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_beacon_seed_fetcher.h"

#include "components/cryptauth/cryptauth_device_manager.h"

namespace cryptauth {

RemoteBeaconSeedFetcher::RemoteBeaconSeedFetcher(
    const CryptAuthDeviceManager* device_manager)
    : device_manager_(device_manager) {}

RemoteBeaconSeedFetcher::~RemoteBeaconSeedFetcher() {}

bool RemoteBeaconSeedFetcher::FetchSeedsForDevice(
    const RemoteDevice& remote_device,
    std::vector<BeaconSeed>* beacon_seeds_out) const {
  if (remote_device.public_key.empty()) {
    return false;
  }

  for(const auto& device_info : device_manager_->GetSyncedDevices()) {
    if (device_info.public_key() == remote_device.public_key) {
      if (device_info.beacon_seeds_size() == 0) {
        return false;
      }

      beacon_seeds_out->clear();
      for (int i = 0; i < device_info.beacon_seeds_size(); i++) {
        beacon_seeds_out->push_back(device_info.beacon_seeds(i));
      }
      return true;
    }
  }

  return false;
}

}  // namespace cryptauth
