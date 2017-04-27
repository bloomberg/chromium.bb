// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_device_prioritizer.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace tether {

HostScanDevicePrioritizer::HostScanDevicePrioritizer(
    TetherHostResponseRecorder* tether_host_response_recorder)
    : tether_host_response_recorder_(tether_host_response_recorder) {}

HostScanDevicePrioritizer::~HostScanDevicePrioritizer() {}

void HostScanDevicePrioritizer::SortByHostScanOrder(
    std::vector<cryptauth::RemoteDevice>* remote_devices) const {
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
  // |remote_devices| vector.
  for (auto prioritized_it = prioritized_ids.rbegin();
       prioritized_it != prioritized_ids.rend(); ++prioritized_it) {
    // Iterate through |remote_devices| to see if a device exists with a
    // device ID of |stored_id|. If one exists, remove it from its previous
    // position in the list and add it at the front instead.
    for (auto remote_devices_it = remote_devices->begin();
         remote_devices_it != remote_devices->end(); ++remote_devices_it) {
      if (remote_devices_it->GetDeviceId() != *prioritized_it) {
        continue;
      }

      cryptauth::RemoteDevice device_to_move = *remote_devices_it;
      remote_devices->erase(remote_devices_it);
      remote_devices->insert(remote_devices->begin(), device_to_move);
      break;
    }
  }
}

}  // namespace tether

}  // namespace chromeos
