// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

const char NetworkConfigurationUpdater::kEmptyConfiguration[] =
    "{\"NetworkConfigurations\":[],\"Certificates\":[]}";

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    ConfigurationPolicyProvider* provider,
    chromeos::NetworkLibrary* network_library)
    : network_library_(network_library) {
  DCHECK(network_library_);
  provider_registrar_.Init(provider, this);
  Update();
}

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {}

void NetworkConfigurationUpdater::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  Update();
}

void NetworkConfigurationUpdater::Update() {
  ConfigurationPolicyProvider* provider = provider_registrar_.provider();
  const PolicyMap& policy = provider->policies().Get(POLICY_DOMAIN_CHROME, "");

  ApplyNetworkConfiguration(policy, key::kDeviceOpenNetworkConfiguration,
                            chromeos::NetworkUIData::ONC_SOURCE_DEVICE_POLICY,
                            &device_network_config_);
  ApplyNetworkConfiguration(policy, key::kOpenNetworkConfiguration,
                            chromeos::NetworkUIData::ONC_SOURCE_USER_POLICY,
                            &user_network_config_);
}

void NetworkConfigurationUpdater::ApplyNetworkConfiguration(
    const PolicyMap& policy_map,
    const char* policy_name,
    chromeos::NetworkUIData::ONCSource onc_source,
    std::string* cached_value) {
  std::string new_network_config;
  const base::Value* value = policy_map.GetValue(policy_name);
  if (value != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!value->GetAsString(&new_network_config))
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
                                           &error)) {
      LOG(WARNING) << "Network library failed to load ONC configuration:"
                   << error;
    }
  }
}

}  // namespace policy
