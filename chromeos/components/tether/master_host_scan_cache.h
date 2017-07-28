// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/host_scan_cache.h"

namespace chromeos {

namespace tether {

class ActiveHost;
class PersistentHostScanCache;
class TimerFactory;

// HostScanCache implementation which interfaces with the network stack as well
// as storing scanned device properties persistently and recovering stored
// properties after a browser crash. When SetHostScanResult() is called,
// MasterHostScanCache starts a timer which automatically removes scan results
// after |kNumMinutesBeforeCacheEntryExpires| minutes.
class MasterHostScanCache : public HostScanCache {
 public:
  // The number of minutes that a cache entry is considered to be valid before
  // it becomes stale. Once a cache entry is inserted, it will be automatically
  // removed after this amount of time passes unless it corresponds to the
  // active host. This value was chosen for two reasons:
  //     (1) Tether properties such as battery percentage and signal strength
  //         are ephemeral in nature, so keeping these values cached for more
  //         than a short period makes it likely that the values will be wrong.
  //     (2) Tether networks rely on proximity to a tether host device, so it is
  //         possible that host devices have physically moved away from each
  //         other. We assume that the devices do not stay in proximity to one
  //         another until a new scan result is received which proves that they
  //         are still within the distance needed to communicate.
  static constexpr int kNumMinutesBeforeCacheEntryExpires = 5;

  MasterHostScanCache(std::unique_ptr<TimerFactory> timer_factory,
                      ActiveHost* active_host,
                      HostScanCache* network_host_scan_cache,
                      PersistentHostScanCache* persistent_host_scan_cache);
  ~MasterHostScanCache() override;

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool ExistsInCache(const std::string& tether_network_guid) override;
  std::unordered_set<std::string> GetTetherGuidsInCache() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

 protected:
  bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) override;

 private:
  friend class MasterHostScanCacheTest;

  void InitializeFromPersistentCache();
  void StartTimer(const std::string& tether_network_guid);
  void OnTimerFired(const std::string& tether_network_guid);

  std::unique_ptr<TimerFactory> timer_factory_;
  ActiveHost* active_host_;
  HostScanCache* network_host_scan_cache_;
  PersistentHostScanCache* persistent_host_scan_cache_;

  bool is_initializing_;

  // Maps from the Tether network GUID to a Timer object. While a scan result is
  // active in the cache, the corresponding Timer object starts running; if the
  // timer fires, the result is removed (unless it corresponds to the active
  // host).
  std::unordered_map<std::string, std::unique_ptr<base::Timer>>
      tether_guid_to_timer_map_;
  base::WeakPtrFactory<MasterHostScanCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MasterHostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_
