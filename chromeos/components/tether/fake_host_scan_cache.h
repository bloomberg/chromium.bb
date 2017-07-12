// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chromeos/components/tether/host_scan_cache.h"

namespace chromeos {

namespace tether {

// Test double for HostScanCache which stores cache results in memory.
class FakeHostScanCache : public HostScanCache {
 public:
  FakeHostScanCache();
  ~FakeHostScanCache() override;

  // Getter and setter for the active host Tether network GUID. This value is
  // used to implement the ClearCacheExceptForActiveHost() function.
  std::string& active_host_tether_network_guid() {
    return active_host_tether_network_guid_;
  }
  void set_active_host_tether_network_guid(
      const std::string& active_host_tether_network_guid) {
    active_host_tether_network_guid_ = active_host_tether_network_guid;
  }

  // Getters for contents of the cache.
  const HostScanCacheEntry* GetCacheEntry(
      const std::string& tether_network_guid);
  size_t size() { return cache_.size(); }
  bool empty() { return cache_.empty(); }
  const std::unordered_map<std::string, HostScanCacheEntry> cache() {
    return cache_;
  }

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool RemoveHostScanResult(const std::string& tether_network_guid) override;
  void ClearCacheExceptForActiveHost() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

 private:
  std::string active_host_tether_network_guid_;
  std::unordered_map<std::string, HostScanCacheEntry> cache_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_
