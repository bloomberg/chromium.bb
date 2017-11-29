// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_configuration_remover.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "components/proximity_auth/logging/logging.h"

namespace {

void RemoveConfigurationSuccessCallback(const std::string& guid) {
  PA_LOG(INFO) << "Successfully removed Wi-Fi network with GUID " << guid
               << ".";
}

void RemoveConfigurationFailureCallback(
    const std::string& guid,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  PA_LOG(WARNING) << "Failed to remove Wi-Fi network with GUID " << guid
                  << ". Error:" << error_name << ".";
}

}  // namespace

namespace chromeos {

namespace tether {

NetworkConfigurationRemover::NetworkConfigurationRemover(
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler)
    : network_state_handler_(network_state_handler),
      managed_network_configuration_handler_(
          managed_network_configuration_handler) {}

NetworkConfigurationRemover::~NetworkConfigurationRemover() = default;

void NetworkConfigurationRemover::RemoveNetworkConfiguration(
    const std::string& wifi_network_guid) {
  managed_network_configuration_handler_->RemoveConfiguration(
      network_state_handler_->GetNetworkStateFromGuid(wifi_network_guid)
          ->path(),
      base::Bind(RemoveConfigurationSuccessCallback, wifi_network_guid),
      base::Bind(RemoveConfigurationFailureCallback, wifi_network_guid));
}

}  // namespace tether

}  // namespace chromeos
