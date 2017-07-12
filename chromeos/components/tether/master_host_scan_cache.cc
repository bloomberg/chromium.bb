// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/master_host_scan_cache.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

MasterHostScanCache::MasterHostScanCache(
    NetworkStateHandler* network_state_handler,
    ActiveHost* active_host,
    TetherHostResponseRecorder* tether_host_response_recorder,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : timer_factory_(base::MakeUnique<TimerFactory>()),
      network_state_handler_(network_state_handler),
      active_host_(active_host),
      tether_host_response_recorder_(tether_host_response_recorder),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      weak_ptr_factory_(this) {
  tether_host_response_recorder_->AddObserver(this);
}

MasterHostScanCache::~MasterHostScanCache() {
  tether_host_response_recorder_->RemoveObserver(this);
}

void MasterHostScanCache::SetHostScanResult(const HostScanCacheEntry& entry) {
  auto found_iter = tether_guid_to_timer_map_.find(entry.tether_network_guid);

  if (found_iter == tether_guid_to_timer_map_.end()) {
    // Add the Tether network to NetworkStateHandler and create an associated
    // Timer.
    network_state_handler_->AddTetherNetworkState(
        entry.tether_network_guid, entry.device_name, entry.carrier,
        entry.battery_percentage, entry.signal_strength,
        HasConnectedToHost(entry.tether_network_guid));
    tether_guid_to_timer_map_.emplace(entry.tether_network_guid,
                                      timer_factory_->CreateOneShotTimer());

    PA_LOG(INFO) << "Added scan result for Tether network with GUID "
                 << entry.tether_network_guid << ". "
                 << "Device name: " << entry.device_name << ", "
                 << "carrier: " << entry.carrier << ", "
                 << "battery percentage: " << entry.battery_percentage << ", "
                 << "signal strength: " << entry.signal_strength;
  } else {
    // Update the existing network and stop the associated Timer.
    network_state_handler_->UpdateTetherNetworkProperties(
        entry.tether_network_guid, entry.carrier, entry.battery_percentage,
        entry.signal_strength);
    found_iter->second->Stop();

    PA_LOG(INFO) << "Updated scan result for Tether network with GUID "
                 << entry.tether_network_guid << ". "
                 << "New carrier: " << entry.carrier << ", "
                 << "new battery percentage: " << entry.battery_percentage
                 << ", new signal strength: " << entry.signal_strength;
  }

  if (entry.setup_required)
    setup_required_tether_guids_.insert(entry.tether_network_guid);
  else
    setup_required_tether_guids_.erase(entry.tether_network_guid);

  StartTimer(entry.tether_network_guid);
}

bool MasterHostScanCache::RemoveHostScanResult(
    const std::string& tether_network_guid) {
  DCHECK(!tether_network_guid.empty());

  auto it = tether_guid_to_timer_map_.find(tether_network_guid);
  if (it == tether_guid_to_timer_map_.end()) {
    PA_LOG(ERROR) << "Attempted to remove a host scan result which does not "
                  << "exist in the cache. GUID: " << tether_network_guid;
    return false;
  }

  if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
    PA_LOG(ERROR) << "RemoveHostScanResult() called for Tether network with "
                  << "GUID " << tether_network_guid << ", but the "
                  << "corresponding device is the active host. Not removing "
                  << "this scan result from the cache.";
    return false;
  }

  tether_guid_to_timer_map_.erase(it);
  setup_required_tether_guids_.erase(tether_network_guid);
  return network_state_handler_->RemoveTetherNetworkState(tether_network_guid);
}

void MasterHostScanCache::ClearCacheExceptForActiveHost() {
  // Create a list of all Tether network GUIDs serving as keys to
  // |tether_guid_to_timer_map_|.
  std::vector<std::string> tether_network_guids;
  tether_network_guids.reserve(tether_guid_to_timer_map_.size());
  for (auto& it : tether_guid_to_timer_map_)
    tether_network_guids.push_back(it.first);

  std::string active_host_tether_guid = active_host_->GetTetherNetworkGuid();
  if (active_host_tether_guid.empty()) {
    PA_LOG(INFO) << "Clearing all " << tether_guid_to_timer_map_.size() << " "
                 << "entries from the cache.";
  } else {
    PA_LOG(INFO) << "Clearing " << (tether_guid_to_timer_map_.size() - 1) << " "
                 << "of the " << tether_guid_to_timer_map_.size() << " "
                 << "entries from the cache. Not removing the entry "
                 << "corresponding to the Tether network with GUID "
                 << active_host_tether_guid << " because it represents the "
                 << "active host.";
  }

  // Iterate through the keys, removing all scan results not corresponding to
  // the active host. Iteration is done via a list of pre-computed keys instead
  // if iterating through the map because RemoteHostScanResult() will remove
  // key/value pairs from the map, which would invalidate the map iterator.
  for (auto& tether_network_guid : tether_network_guids) {
    if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
      // Do not remove the active host from the cache.
      continue;
    }

    RemoveHostScanResult(tether_network_guid);
  }
}

bool MasterHostScanCache::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  return setup_required_tether_guids_.find(tether_network_guid) !=
         setup_required_tether_guids_.end();
}

void MasterHostScanCache::OnPreviouslyConnectedHostIdsChanged() {
  for (auto& map_entry : tether_guid_to_timer_map_) {
    const std::string& tether_network_guid = map_entry.first;
    if (!HasConnectedToHost(tether_network_guid))
      continue;

    // If a the current device has connected to the Tether network with GUID
    // |tether_network_guid|, alert |network_state_handler_|. Note that this
    // function is a no-op if it is called on a network which already has its
    // HasConnectedToHost property set to true.
    bool update_successful =
        network_state_handler_->SetTetherNetworkHasConnectedToHost(
            tether_network_guid);

    if (update_successful) {
      PA_LOG(INFO) << "Successfully set the HasConnectedToHost property of "
                   << "the Tether network with GUID " << tether_network_guid
                   << " to true.";
    }
  }
}

void MasterHostScanCache::SetTimerFactoryForTest(
    std::unique_ptr<TimerFactory> timer_factory_for_test) {
  timer_factory_ = std::move(timer_factory_for_test);
}

bool MasterHostScanCache::HasConnectedToHost(
    const std::string& tether_network_guid) {
  std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);
  std::vector<std::string> connected_device_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  return std::find(connected_device_ids.begin(), connected_device_ids.end(),
                   device_id) != connected_device_ids.end();
}

void MasterHostScanCache::StartTimer(const std::string& tether_network_guid) {
  auto found_iter = tether_guid_to_timer_map_.find(tether_network_guid);
  DCHECK(found_iter != tether_guid_to_timer_map_.end());
  DCHECK(!found_iter->second->IsRunning());

  PA_LOG(INFO) << "Starting host scan cache timer for Tether network with GUID "
               << tether_network_guid << ". Will fire in "
               << kNumMinutesBeforeCacheEntryExpires << " minutes.";

  found_iter->second->Start(
      FROM_HERE,
      base::TimeDelta::FromMinutes(kNumMinutesBeforeCacheEntryExpires),
      base::Bind(&MasterHostScanCache::OnTimerFired,
                 weak_ptr_factory_.GetWeakPtr(), tether_network_guid));
}

void MasterHostScanCache::OnTimerFired(const std::string& tether_network_guid) {
  if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
    // Log as a warning. This situation should be uncommon in practice since
    // KeepAliveScheduler should schedule a new keep-alive status update every
    // 4 minutes.
    PA_LOG(WARNING) << "Timer fired for Tether network GUID "
                    << tether_network_guid << ", but the corresponding device "
                    << "is the active host. Restarting timer.";

    // If the Timer which fired corresponds to the active host, do not remove
    // the cache entry. The active host must always remain in the cache so that
    // the UI can reflect that it is the connecting/connected network. In this
    // case, just restart the timer.
    StartTimer(tether_network_guid);
    return;
  }

  PA_LOG(INFO) << "Timer fired for Tether network GUID " << tether_network_guid
               << ". Removing stale scan result.";
  RemoveHostScanResult(tether_network_guid);
}

}  // namespace tether

}  // namespace chromeos
