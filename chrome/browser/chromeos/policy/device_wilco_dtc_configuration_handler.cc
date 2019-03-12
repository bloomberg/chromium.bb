// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_wilco_dtc_configuration_handler.h"

#include "components/policy/policy_constants.h"

namespace policy {

DeviceWilcoDtcConfigurationHandler::DeviceWilcoDtcConfigurationHandler(
    PolicyService* policy_service)
    : device_wilco_dtc_configuration_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceWilcoDtcConfiguration,
              this)) {}

DeviceWilcoDtcConfigurationHandler::~DeviceWilcoDtcConfigurationHandler() {}

void DeviceWilcoDtcConfigurationHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  // TODO(b/123933434): handle a data cleared event.
}

void DeviceWilcoDtcConfigurationHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  // TODO(b/123933434): handle |data|.
}

void DeviceWilcoDtcConfigurationHandler::Shutdown() {
  device_wilco_dtc_configuration_observer_.reset();
}

}  // namespace policy
