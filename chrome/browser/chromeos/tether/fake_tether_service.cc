// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/fake_tether_service.h"

constexpr char kTetherGuidPrefix[] = "tether-guid-";
constexpr char kTetherNamePrefix[] = "tether";
constexpr char kCarrier[] = "FakeCarrier";

FakeTetherService::FakeTetherService(
    Profile* profile,
    chromeos::PowerManagerClient* power_manager_client,
    chromeos::device_sync::DeviceSyncClient* device_sync_client,
    chromeos::secure_channel::SecureChannelClient* secure_channel_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client,
    chromeos::NetworkStateHandler* network_state_handler,
    session_manager::SessionManager* session_manager)
    : TetherService(profile,
                    power_manager_client,
                    device_sync_client,
                    secure_channel_client,
                    multidevice_setup_client,
                    network_state_handler,
                    session_manager) {}

void FakeTetherService::StartTetherIfPossible() {
  if (GetTetherTechnologyState() !=
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  for (int i = 0; i < num_tether_networks_; ++i) {
    network_state_handler()->AddTetherNetworkState(
        kTetherGuidPrefix + std::to_string(i),
        kTetherNamePrefix + std::to_string(i), kCarrier,
        100 /* battery_percentage */, 100 /* signal_strength */,
        false /* has_connected_to_host */);
  }
}

void FakeTetherService::StopTetherIfNecessary() {
  for (int i = 0; i < num_tether_networks_; ++i) {
    network_state_handler()->RemoveTetherNetworkState(kTetherGuidPrefix + i);
  }
}

bool FakeTetherService::HasSyncedTetherHosts() const {
  return true;
}
