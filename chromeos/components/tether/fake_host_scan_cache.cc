// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_host_scan_cache.h"

namespace chromeos {

namespace tether {

FakeHostScanCache::FakeHostScanCache() : HostScanCache() {}

FakeHostScanCache::~FakeHostScanCache() {}

const HostScanCacheEntry* FakeHostScanCache::GetCacheEntry(
    const std::string& tether_network_guid) {
  auto it = cache_.find(tether_network_guid);
  if (it == cache_.end())
    return nullptr;

  return &it->second;
}

void FakeHostScanCache::SetHostScanResult(const HostScanCacheEntry& entry) {
  // Erase any existing entry with the same GUID if it exists (if nothing
  // currently exists with that GUID, this is a no-op).
  cache_.erase(entry.tether_network_guid);

  // Add the new entry.
  cache_.emplace(entry.tether_network_guid, entry);
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

bool FakeHostScanCache::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  auto it = cache_.find(tether_network_guid);
  if (it != cache_.end())
    return it->second.setup_required;

  return false;
}

}  // namespace tether

}  // namespace chromeos
