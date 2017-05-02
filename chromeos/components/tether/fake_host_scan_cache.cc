// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_host_scan_cache.h"

#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

namespace tether {

namespace {

// Shared among all FakeHostScanCache instances. This pointer is not used for
// anything, but HostScanCache's constructor calls AddObserver() on the
// TetherHostResponseRecorder* passed to its constructor, so a valid object must
// be passed to the constructor to prevent a segfault.
TetherHostResponseRecorder* kTetherHostResponseRecorder = nullptr;

TetherHostResponseRecorder* GetTetherHostResponseRecorder() {
  if (kTetherHostResponseRecorder)
    return kTetherHostResponseRecorder;

  kTetherHostResponseRecorder =
      new TetherHostResponseRecorder(nullptr /* pref_service */);
  return kTetherHostResponseRecorder;
}

}  // namespace

FakeHostScanCache::FakeHostScanCache()
    : HostScanCache(nullptr /* network_state_handler */,
                    nullptr /* active_host */,
                    GetTetherHostResponseRecorder(),
                    nullptr /* device_id_tether_network_guid_map */) {}

FakeHostScanCache::~FakeHostScanCache() {}

const FakeHostScanCache::CacheEntry* FakeHostScanCache::GetCacheEntry(
    const std::string& tether_network_guid) {
  auto it = cache_.find(tether_network_guid);
  if (it == cache_.end())
    return nullptr;

  return &it->second;
}

void FakeHostScanCache::SetHostScanResult(
    const std::string& tether_network_guid,
    const std::string& device_name,
    const std::string& carrier,
    int battery_percentage,
    int signal_strength) {
  auto it = cache_.find(tether_network_guid);
  if (it != cache_.end()) {
    // If already in the cache, update the cache with new values.
    it->second.device_name = device_name;
    it->second.carrier = carrier;
    it->second.battery_percentage = battery_percentage;
    it->second.signal_strength = signal_strength;
    return;
  }

  // Otherwise, add a new entry.
  cache_.emplace(
      tether_network_guid,
      CacheEntry{device_name, carrier, battery_percentage, signal_strength});
}

bool FakeHostScanCache::RemoveHostScanResult(
    const std::string& tether_network_guid) {
  if (tether_network_guid == active_host_tether_network_guid())
    return false;

  return cache_.erase(tether_network_guid) > 0;
}

void FakeHostScanCache::ClearCacheExceptForActiveHost() {
  auto it = cache_.begin();
  while (it != cache_.end()) {
    if (it->first == active_host_tether_network_guid_)
      it++;
    else
      it = cache_.erase(it);
  }
}

void FakeHostScanCache::OnPreviouslyConnectedHostIdsChanged() {}

}  // namespace tether

}  // namespace chromeos
