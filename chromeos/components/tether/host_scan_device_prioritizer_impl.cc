// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_device_prioritizer_impl.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace tether {

namespace {

cryptauth::RemoteDevice Move(const cryptauth::RemoteDevice& remote_device) {
  return remote_device;
}

std::unique_ptr<ManagedState> Move(
    std::unique_ptr<ManagedState>& tether_network_state) {
  return std::move(tether_network_state);
}

}  // namespace

HostScanDevicePrioritizerImpl::HostScanDevicePrioritizerImpl(
    TetherHostResponseRecorder* tether_host_response_recorder,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : tether_host_response_recorder_(tether_host_response_recorder),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map) {}

HostScanDevicePrioritizerImpl::~HostScanDevicePrioritizerImpl() {}

void HostScanDevicePrioritizerImpl::SortByHostScanOrder(
    std::vector<cryptauth::RemoteDevice>* remote_devices) const {
  SortNetworks(remote_devices);
}

void HostScanDevicePrioritizerImpl::SortTetherNetworkList(
    NetworkStateHandler::ManagedStateList* tether_networks) const {
  SortNetworks(tether_networks);
}

template <typename T>
void HostScanDevicePrioritizerImpl::SortNetworks(
    std::vector<T>* list_to_sort) const {
  // First, fetch the hosts which have previously responded.
  std::vector<std::string> prioritized_ids =
      tether_host_response_recorder_->GetPreviouslyAvailableHostIds();

  std::vector<std::string> previously_connectable_host_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  if (!previously_connectable_host_ids.empty()) {
    // If there is a most-recently connectable host, insert it at the front of
    // the list.
    prioritized_ids.insert(prioritized_ids.begin(),
                           previously_connectable_host_ids[0]);
  }

  // Iterate from the last stored ID to the first stored ID. This ensures that
  // the items at the front of the list end up in the front of the prioritized
  // |list_to_sort| vector.
  for (auto prioritized_it = prioritized_ids.rbegin();
       prioritized_it != prioritized_ids.rend(); ++prioritized_it) {
    // Iterate through |list_to_sort| to see if a device ID exists which is
    // equal to |stored_id|. If one exists, remove it from its previous
    // position in the list and add it at the front instead.
    for (auto list_to_sort_it = list_to_sort->begin();
         list_to_sort_it != list_to_sort->end(); ++list_to_sort_it) {
      if (GetDeviceId(*list_to_sort_it) != *prioritized_it) {
        continue;
      }

      T entry_to_move = Move(*list_to_sort_it);
      list_to_sort->erase(list_to_sort_it);
      list_to_sort->emplace(list_to_sort->begin(), Move(entry_to_move));
      break;
    }
  }
}

std::string HostScanDevicePrioritizerImpl::GetDeviceId(
    const cryptauth::RemoteDevice& remote_device) const {
  return remote_device.GetDeviceId();
}

std::string HostScanDevicePrioritizerImpl::GetDeviceId(
    const std::unique_ptr<ManagedState>& tether_network_state) const {
  NetworkState* network_state =
      static_cast<NetworkState*>(tether_network_state.get());
  return device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
      network_state->guid());
}

}  // namespace tether

}  // namespace chromeos
