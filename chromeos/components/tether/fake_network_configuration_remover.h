// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_

#include <string>

#include "base/macros.h"
#include "chromeos/components/tether/network_configuration_remover.h"

namespace chromeos {

namespace tether {

// Test double for NetworkConfigurationRemover.
class FakeNetworkConfigurationRemover : public NetworkConfigurationRemover {
 public:
  FakeNetworkConfigurationRemover();
  ~FakeNetworkConfigurationRemover() override;

  std::string last_removed_wifi_network_guid() {
    return last_removed_wifi_network_guid_;
  }

  // NetworkConfigurationRemover:
  void RemoveNetworkConfiguration(
      const std::string& wifi_network_guid) override;

 private:
  std::string last_removed_wifi_network_guid_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkConfigurationRemover);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_
