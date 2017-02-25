// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_device_prioritizer.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/tether/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace tether {

// static
void HostScanDevicePrioritizer::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kMostRecentTetherAvailablilityResponderIds);
  registry->RegisterStringPref(prefs::kMostRecentConnectTetheringResponderId,
                               "");
}

HostScanDevicePrioritizer::HostScanDevicePrioritizer(PrefService* pref_service)
    : pref_service_(pref_service) {}

HostScanDevicePrioritizer::~HostScanDevicePrioritizer() {}

void HostScanDevicePrioritizer::RecordSuccessfulTetherAvailabilityResponse(
    const cryptauth::RemoteDevice& remote_device) {
  std::string device_id = remote_device.GetDeviceId();

  const base::ListValue* ids =
      pref_service_->GetList(prefs::kMostRecentTetherAvailablilityResponderIds);

  // Create a mutable copy of the stored IDs, or create one if it has yet to be
  // stored.
  std::unique_ptr<base::ListValue> updated_ids =
      ids ? ids->CreateDeepCopy() : base::MakeUnique<base::ListValue>();

  // Remove the device ID if it was already present in the list.
  std::unique_ptr<base::Value> device_id_value =
      base::MakeUnique<base::Value>(device_id);
  updated_ids->Remove(*device_id_value, nullptr);

  // Add the device ID to the front of the queue.
  updated_ids->Insert(0, std::move(device_id_value));

  // Store the updated list back in |pref_service_|.
  pref_service_->Set(prefs::kMostRecentTetherAvailablilityResponderIds,
                     *updated_ids);
}

void HostScanDevicePrioritizer::RecordSuccessfulConnectTetheringResponse(
    const cryptauth::RemoteDevice& remote_device) {
  pref_service_->Set(prefs::kMostRecentConnectTetheringResponderId,
                     base::Value(remote_device.GetDeviceId()));
}

void HostScanDevicePrioritizer::SortByHostScanOrder(
    std::vector<cryptauth::RemoteDevice>* remote_devices) const {
  // Fetch the stored IDs associated with the devices which most recently sent
  // TetherAvailabilityResponses.
  const base::ListValue* tether_availability_ids =
      pref_service_->GetList(prefs::kMostRecentTetherAvailablilityResponderIds);

  // Create a mutable copy of the stored IDs, or create one if it has yet to be
  // stored.
  std::unique_ptr<base::ListValue> prioritized_ids =
      tether_availability_ids ? tether_availability_ids->CreateDeepCopy()
                              : base::MakeUnique<base::ListValue>();

  // Now, fetch the ID associated with the device which most recently sent a
  // ConnectTetheringRequest.
  std::string connect_tethering_id =
      pref_service_->GetString(prefs::kMostRecentConnectTetheringResponderId);

  // If an ID exists, insert it at the front of |prioritized_ids|.
  if (!connect_tethering_id.empty()) {
    prioritized_ids->Insert(
        0, base::MakeUnique<base::Value>(connect_tethering_id));
  }

  // Iterate from the last stored ID to the first stored ID. This ensures that
  // the items at the front of the list end up in the front of the prioritized
  // |remote_devices| vector.
  for (size_t i = prioritized_ids->GetSize(); i-- > 0;) {
    base::Value* stored_id_value;
    if (!prioritized_ids->Get(i, &stored_id_value)) {
      continue;
    }

    std::string stored_id;
    if (!stored_id_value->GetAsString(&stored_id)) {
      continue;
    }

    // Iterate through |remote_devices| to see if a device exists with a
    // device ID of |stored_id|. If one exists, remove it from its previous
    // position in the list and add it at the front instead.
    for (auto it = remote_devices->begin(); it != remote_devices->end(); ++it) {
      if (it->GetDeviceId() != stored_id) {
        continue;
      }

      cryptauth::RemoteDevice device_to_move = *it;
      remote_devices->erase(it);
      remote_devices->insert(remote_devices->begin(), device_to_move);
      break;
    }
  }
}

}  // namespace tether

}  // namespace chromeos
