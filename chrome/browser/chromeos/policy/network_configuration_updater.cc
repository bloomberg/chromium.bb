// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    PolicyService* policy_service,
    chromeos::NetworkLibrary* network_library)
    : policy_change_registrar_(
          policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      network_library_(network_library),
      user_policy_initialized_(false),
      allow_web_trust_(false),
      policy_service_(policy_service) {
  DCHECK(network_library_);
  policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_DEVICE_POLICY));
  policy_change_registrar_.Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_USER_POLICY));

  network_library_->AddNetworkProfileObserver(this);

  // Apply the current policies immediately.
  ApplyNetworkConfigurations();
}

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  network_library_->RemoveNetworkProfileObserver(this);
}

void NetworkConfigurationUpdater::OnProfileListChanged() {
  VLOG(1) << "Network profile list changed, applying policies.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdater::OnUserPolicyInitialized() {
  VLOG(1) << "User policy initialized, applying policies.";
  user_policy_initialized_ = true;
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdater::OnPolicyChanged(
    chromeos::onc::ONCSource onc_source,
    const base::Value* previous,
    const base::Value* current) {
  VLOG(1) << "Policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source) << " changed.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdater::ApplyNetworkConfigurations() {
  ApplyNetworkConfiguration(key::kDeviceOpenNetworkConfiguration,
                            chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
  if (user_policy_initialized_) {
    ApplyNetworkConfiguration(key::kOpenNetworkConfiguration,
                              chromeos::onc::ONC_SOURCE_USER_POLICY);
  }
}

void NetworkConfigurationUpdater::ApplyNetworkConfiguration(
    const std::string& policy_key,
    chromeos::onc::ONCSource onc_source) {
  VLOG(1) << "Apply policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source);
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key);

  std::string new_network_config;
  if (policy_value != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!policy_value->GetAsString(&new_network_config)) {
      LOG(WARNING) << "ONC policy for source "
                   << chromeos::onc::GetSourceAsString(onc_source)
                   << " is not a string value.";
    }
  }

  // An empty string is not a valid ONC and generates warnings and
  // errors. Replace by a valid empty configuration.
  if (new_network_config.empty())
    new_network_config = chromeos::onc::kEmptyUnencryptedConfiguration;

  if (!network_library_->LoadOncNetworks(new_network_config, "", onc_source,
                                         allow_web_trust_)) {
    LOG(ERROR) << "Errors occurred during the ONC policy application.";
  }
}

}  // namespace policy
