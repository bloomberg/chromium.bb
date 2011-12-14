// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

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

  PolicyMap policy;
  if (!provider->Provide(&policy)) {
    LOG(WARNING) << "Failed to read policy from policy provider.";
    return;
  }

  ApplyNetworkConfiguration(policy, kPolicyDeviceOpenNetworkConfiguration,
                            &device_network_config_);
  ApplyNetworkConfiguration(policy, kPolicyOpenNetworkConfiguration,
                            &user_network_config_);
}

void NetworkConfigurationUpdater::ApplyNetworkConfiguration(
    const PolicyMap& policy_map,
    ConfigurationPolicyType policy_type,
    std::string* cached_value) {
  std::string new_network_config;
  const base::Value* value = policy_map.Get(policy_type);
  if (value != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!value->GetAsString(&new_network_config))
      LOG(WARNING) << "Invalid network configuration.";
  }

  if (*cached_value != new_network_config) {
    *cached_value = new_network_config;
    if (!network_library_->LoadOncNetworks(new_network_config, ""))
      LOG(WARNING) << "Network library failed to load ONC configuration.";
  }
}

}  // namespace policy
