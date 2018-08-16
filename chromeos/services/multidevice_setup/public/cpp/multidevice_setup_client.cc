// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace chromeos {

namespace multidevice_setup {

MultiDeviceSetupClient::MultiDeviceSetupClient() = default;

MultiDeviceSetupClient::~MultiDeviceSetupClient() = default;

void MultiDeviceSetupClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MultiDeviceSetupClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MultiDeviceSetupClient::NotifyHostStatusChanged(
    mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  for (auto& observer : observer_list_)
    observer.OnHostStatusChanged(host_status, host_device);
}

void MultiDeviceSetupClient::NotifyFeatureStateChanged(
    const FeatureStatesMap& feature_states_map) {
  for (auto& observer : observer_list_)
    observer.OnFeatureStatesChanged(feature_states_map);
}

}  // namespace multidevice_setup

}  // namespace chromeos
