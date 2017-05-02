// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_response_recorder.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/tether/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace tether {

// static
void TetherHostResponseRecorder::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kMostRecentTetherAvailablilityResponderIds);
  registry->RegisterListPref(prefs::kMostRecentConnectTetheringResponderIds);
}

TetherHostResponseRecorder::TetherHostResponseRecorder(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

TetherHostResponseRecorder::~TetherHostResponseRecorder() {}

void TetherHostResponseRecorder::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TetherHostResponseRecorder::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TetherHostResponseRecorder::RecordSuccessfulTetherAvailabilityResponse(
    const cryptauth::RemoteDevice& remote_device) {
  AddRecentResponse(remote_device.GetDeviceId(),
                    prefs::kMostRecentTetherAvailablilityResponderIds);
}

std::vector<std::string>
TetherHostResponseRecorder::GetPreviouslyAvailableHostIds() const {
  return GetDeviceIdsForPref(prefs::kMostRecentTetherAvailablilityResponderIds);
}

void TetherHostResponseRecorder::RecordSuccessfulConnectTetheringResponse(
    const cryptauth::RemoteDevice& remote_device) {
  if (AddRecentResponse(remote_device.GetDeviceId(),
                        prefs::kMostRecentConnectTetheringResponderIds)) {
    NotifyObserversPreviouslyConnectedHostIdsChanged();
  }
}

std::vector<std::string>
TetherHostResponseRecorder::GetPreviouslyConnectedHostIds() const {
  return GetDeviceIdsForPref(prefs::kMostRecentConnectTetheringResponderIds);
}

void TetherHostResponseRecorder::
    NotifyObserversPreviouslyConnectedHostIdsChanged() {
  for (Observer& observer : observer_list_) {
    observer.OnPreviouslyConnectedHostIdsChanged();
  }
}

bool TetherHostResponseRecorder::AddRecentResponse(
    const std::string& device_id,
    const std::string& pref_name) {
  const base::ListValue* ids = pref_service_->GetList(pref_name);

  std::string first_device_id_in_list;
  ids->GetString(0u, &first_device_id_in_list);
  if (device_id == first_device_id_in_list) {
    // If the device ID that is being inserted is already at the front of the
    // list, there is nothing to do.
    return false;
  }

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
  pref_service_->Set(pref_name, *updated_ids);

  return true;
}

std::vector<std::string> TetherHostResponseRecorder::GetDeviceIdsForPref(
    const std::string& pref_name) const {
  std::vector<std::string> device_ids;

  const base::ListValue* ids = pref_service_->GetList(pref_name);
  if (!ids)
    return device_ids;

  for (auto it = ids->begin(); it != ids->end(); ++it) {
    std::string device_id;
    bool success = it->GetAsString(&device_id);
    if (success)
      device_ids.push_back(device_id);
  }

  return device_ids;
}

}  // namespace tether

}  // namespace chromeos
