// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class ActiveHost;
class DeviceIdTetherNetworkGuidMap;
class TetherHostResponseRecorder;

// Caches scan results and inserts them into the network stack.
class HostScanCache : public TetherHostResponseRecorder::Observer {
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

  HostScanCache(
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      TetherHostResponseRecorder* tether_host_response_recorder,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);
  virtual ~HostScanCache();

  // Updates the cache to include this scan result. If no scan result for
  // |tether_network_guid| exists in the cache, a scan result will be added;
  // if a scan result is already present, it is updated with the new data
  // provided as parameters to this function. Once scan results have been
  // updated, a timer starts counting down |kNumMinutesBeforeCacheEntryExpires|
  // minutes. Once the timer fires, the scan result is automatically removed
  // from the cache unless it corresponds to the active host.
  // Note: |signal_strength| should be in the range [0, 100]. This is different
  // from the |connection_strength| field received in ConnectTetheringResponse
  // and KeepAliveTickleResponse messages (the range is [0, 4] in those cases).
  virtual void SetHostScanResult(const std::string& tether_network_guid,
                                 const std::string& device_name,
                                 const std::string& carrier,
                                 int battery_percentage,
                                 int signal_strength);

  // Removes the scan result with GUID |tether_network_guid| from the cache. If
  // no cache result with that GUID was present in the cache, this function is
  // a no-op. Returns whether a scan result was actually removed.
  virtual bool RemoveHostScanResult(const std::string& tether_network_guid);

  // Removes all scan results from the cache unless they correspond to the
  // active host; the active host must always remain in the cache while
  // connecting/connected to ensure the UI is up to date.
  virtual void ClearCacheExceptForActiveHost();

  // TetherHostResponseRecorder::Observer:
  void OnPreviouslyConnectedHostIdsChanged() override;

  class TimerFactory {
   public:
    virtual std::unique_ptr<base::Timer> CreateOneShotTimer() = 0;
  };

 private:
  friend class HostScanCacheTest;

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
  base::WeakPtrFactory<HostScanCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_
