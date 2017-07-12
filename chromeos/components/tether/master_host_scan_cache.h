// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class ActiveHost;
class DeviceIdTetherNetworkGuidMap;
class TetherHostResponseRecorder;
class TimerFactory;

// Master host scan cache, which stores host scan results in the network stack.
// TODO(khorimoto): Add the ability to store tether host scan results
// persistently so that they can be recovered after a browser crash.
class MasterHostScanCache : public HostScanCache,
                            public TetherHostResponseRecorder::Observer {
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

  MasterHostScanCache(
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      TetherHostResponseRecorder* tether_host_response_recorder,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);
  ~MasterHostScanCache() override;

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool RemoveHostScanResult(const std::string& tether_network_guid) override;
  void ClearCacheExceptForActiveHost() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

  // TetherHostResponseRecorder::Observer:
  void OnPreviouslyConnectedHostIdsChanged() override;

 private:
  friend class MasterHostScanCacheTest;

  void SetTimerFactoryForTest(
      std::unique_ptr<TimerFactory> timer_factory_for_test);

  bool HasConnectedToHost(const std::string& tether_network_guid);
  void StartTimer(const std::string& tether_network_guid);
  void OnTimerFired(const std::string& tether_network_guid);

  std::unique_ptr<TimerFactory> timer_factory_;
  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;

  // Maps from the Tether network GUID to a Timer object. While a scan result is
  // active in the cache, the corresponding Timer object starts running; if the
  // timer fires, the result is removed (unless it corresponds to the active
  // host).
  std::unordered_map<std::string, std::unique_ptr<base::Timer>>
      tether_guid_to_timer_map_;
  std::unordered_set<std::string> setup_required_tether_guids_;
  base::WeakPtrFactory<MasterHostScanCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MasterHostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MASTER_HOST_SCAN_CACHE_H_
