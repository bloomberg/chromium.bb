// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "policy/policy_constants.h"

namespace policy {

DeviceNetworkConfigurationUpdater::~DeviceNetworkConfigurationUpdater() {}

// static
scoped_ptr<DeviceNetworkConfigurationUpdater>
DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings) {
  scoped_ptr<DeviceNetworkConfigurationUpdater> updater(
      new DeviceNetworkConfigurationUpdater(certificate_importer.Pass(),
                                            policy_service,
                                            network_config_handler,
                                            network_device_handler,
                                            cros_settings));
  updater->Init();
  return updater.Pass();
}

DeviceNetworkConfigurationUpdater::DeviceNetworkConfigurationUpdater(
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_DEVICE_POLICY,
                                  key::kDeviceOpenNetworkConfiguration,
                                  certificate_importer.Pass(),
                                  policy_service,
                                  network_config_handler),
      network_device_handler_(network_device_handler),
      cros_settings_(cros_settings),
      weak_factory_(this) {
  DCHECK(network_device_handler_);
  data_roaming_setting_subscription_ = cros_settings->AddSettingsObserver(
      chromeos::kSignedDataRoamingEnabled,
      base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          base::Unretained(this)));
}

void DeviceNetworkConfigurationUpdater::Init() {
  NetworkConfigurationUpdater::Init();

  // Apply the roaming setting initially.
  OnDataRoamingSettingChanged();
}

void DeviceNetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  certificate_importer_->ImportCertificates(
      certificates_onc, onc_source_, NULL);
}

void DeviceNetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  network_config_handler_->SetPolicy(onc_source_,
                                     std::string() /* no username hash */,
                                     *network_configs_onc,
                                     *global_network_config);
}

void DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus trusted_status =
      cros_settings_->PrepareTrustedValues(base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          weak_factory_.GetWeakPtr()));

  if (trusted_status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Return, this function will be called again later by
    // PrepareTrustedValues.
    return;
  }

  bool data_roaming_setting = false;
  if (trusted_status == chromeos::CrosSettingsProvider::TRUSTED) {
    if (!cros_settings_->GetBoolean(chromeos::kSignedDataRoamingEnabled,
                                    &data_roaming_setting)) {
      LOG(ERROR) << "Couldn't get device setting "
                 << chromeos::kSignedDataRoamingEnabled;
      data_roaming_setting = false;
    }
  } else {
    DCHECK_EQ(trusted_status,
              chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
    // Roaming is disabled as we can't determine the correct setting.
  }

  network_device_handler_->SetCellularAllowRoaming(data_roaming_setting);
}

}  // namespace policy
