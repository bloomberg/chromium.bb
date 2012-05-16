// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace chromeos {
class NetworkLibrary;
}

namespace policy {

class PolicyMap;

// Keeps track of the network configuration policy settings and updates the
// network definitions whenever the configuration changes.
class NetworkConfigurationUpdater
    : public ConfigurationPolicyProvider::Observer {
 public:
  NetworkConfigurationUpdater(ConfigurationPolicyProvider* provider,
                              chromeos::NetworkLibrary* network_library);
  virtual ~NetworkConfigurationUpdater();

  // ConfigurationPolicyProvider::Observer:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;

  // Empty network configuration blob.
  static const char kEmptyConfiguration[];

 private:
  // Grabs network configuration from policy and applies it.
  void Update();

  // Extracts ONC string from |policy_map| and pushes the configuration to
  // |network_library_| if it's different from |*cached_value| (which is
  // updated).
  void ApplyNetworkConfiguration(const PolicyMap& policy_map,
                                 const char* policy_name,
                                 chromeos::NetworkUIData::ONCSource onc_source,
                                 std::string* cached_value);

  // Wraps the provider we read network configuration from.
  ConfigurationPolicyObserverRegistrar provider_registrar_;

  // Network library to write network configuration to.
  chromeos::NetworkLibrary* network_library_;

  // Current settings.
  std::string device_network_config_;
  std::string user_network_config_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
