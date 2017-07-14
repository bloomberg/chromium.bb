// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/values.h"
#include "chromeos/components/tether/host_scan_cache.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace tether {

// HostScanCache implementation which stores scan results in persistent user
// prefs.
class PersistentHostScanCache : public HostScanCache {
 public:
  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  PersistentHostScanCache(PrefService* pref_service);
  ~PersistentHostScanCache() override;

  // Returns the cache entries that are currently stored in user prefs as a map
  // from Tether network GUID to entry.
  std::unordered_map<std::string, HostScanCacheEntry> GetStoredCacheEntries();

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool RemoveHostScanResult(const std::string& tether_network_guid) override;
  void ClearCacheExceptForActiveHost() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

 private:
  void StoreCacheEntriesToPrefs(
      const std::unordered_map<std::string, HostScanCacheEntry>& entries);

  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(PersistentHostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_
