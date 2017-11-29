// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_network_configuration_remover.h"

namespace chromeos {

namespace tether {

FakeNetworkConfigurationRemover::FakeNetworkConfigurationRemover()
    : NetworkConfigurationRemover(
          nullptr /* network_state_handler */,
          nullptr /* managed_network_configuration_handler */) {}

FakeNetworkConfigurationRemover::~FakeNetworkConfigurationRemover() = default;

void FakeNetworkConfigurationRemover::RemoveNetworkConfiguration(
    const std::string& wifi_network_guid) {
  last_removed_wifi_network_guid_ = wifi_network_guid;
}

}  // namespace tether

}  // namespace chromeos
