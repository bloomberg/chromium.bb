// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/mock_remote_beacon_seed_fetcher.h"

#include "components/cryptauth/cryptauth_device_manager.h"

namespace cryptauth {

MockRemoteBeaconSeedFetcher::MockRemoteBeaconSeedFetcher()
    : RemoteBeaconSeedFetcher(nullptr) {}

MockRemoteBeaconSeedFetcher::~MockRemoteBeaconSeedFetcher() {}

bool MockRemoteBeaconSeedFetcher::FetchSeedsForDevice(
    const RemoteDevice& remote_device,
    std::vector<BeaconSeed>* beacon_seeds_out) const {
  const auto& seeds_iter =
      public_key_to_beacon_seeds_map_.find(remote_device.public_key);
  if (seeds_iter == public_key_to_beacon_seeds_map_.end()) {
    return false;
  }

  *beacon_seeds_out = seeds_iter->second;
  return true;
}

void MockRemoteBeaconSeedFetcher::SetSeedsForDevice(
    const RemoteDevice& remote_device,
    const std::vector<BeaconSeed>* beacon_seeds) {
  if (!beacon_seeds) {
    const auto& it =
        public_key_to_beacon_seeds_map_.find(remote_device.public_key);
    if (it != public_key_to_beacon_seeds_map_.end()) {
      public_key_to_beacon_seeds_map_.erase(it);
    }
    return;
  }

  public_key_to_beacon_seeds_map_[remote_device.public_key] = *beacon_seeds;
}

}  // namespace cryptauth
