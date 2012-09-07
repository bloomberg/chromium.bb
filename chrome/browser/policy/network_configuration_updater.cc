// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

const char NetworkConfigurationUpdater::kEmptyConfiguration[] =
    "{\"NetworkConfigurations\":[],\"Certificates\":[]}";

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    PolicyService* policy_service,
    chromeos::NetworkLibrary* network_library)
    : policy_change_registrar_(
          policy_service, POLICY_DOMAIN_CHROME, std::string()),
      network_library_(network_library),
      allow_web_trust_(false) {
  DCHECK(network_library_);
  policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::ApplyNetworkConfiguration,
                 base::Unretained(this),
                 chromeos::NetworkUIData::ONC_SOURCE_DEVICE_POLICY,
                 &device_network_config_));
  policy_change_registrar_.Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::ApplyNetworkConfiguration,
                 base::Unretained(this),
                 chromeos::NetworkUIData::ONC_SOURCE_USER_POLICY,
                 &user_network_config_));

  // Apply the current values immediately.
  const PolicyMap& policies = policy_service->GetPolicies(POLICY_DOMAIN_CHROME,
                                                          std::string());
  ApplyNetworkConfiguration(
      chromeos::NetworkUIData::ONC_SOURCE_DEVICE_POLICY,
      &device_network_config_,
      NULL,
      policies.GetValue(key::kDeviceOpenNetworkConfiguration));
  ApplyNetworkConfiguration(
      chromeos::NetworkUIData::ONC_SOURCE_USER_POLICY,
      &user_network_config_,
      NULL,
      policies.GetValue(key::kOpenNetworkConfiguration));
}

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {}

void NetworkConfigurationUpdater::ApplyNetworkConfiguration(
    chromeos::NetworkUIData::ONCSource onc_source,
    std::string* cached_value,
    const base::Value* previous,
    const base::Value* current) {
  std::string new_network_config;
  if (current != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!current->GetAsString(&new_network_config))
      LOG(WARNING) << "Invalid network configuration.";
  }

  // We need to load an empty configuration to get rid of any configuration
  // that has been installed previously. An empty string also works, but
  // generates warnings and errors, which we'd like to avoid.
  if (new_network_config.empty())
    new_network_config = kEmptyConfiguration;

  if (*cached_value != new_network_config) {
    *cached_value = new_network_config;
    std::string error;
    if (!network_library_->LoadOncNetworks(new_network_config, "", onc_source,
                                           allow_web_trust_, &error)) {
      LOG(WARNING) << "Network library failed to load ONC configuration:"
                   << error;
    }
  }
}

}  // namespace policy
