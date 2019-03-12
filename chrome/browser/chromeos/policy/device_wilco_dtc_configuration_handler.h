// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WILCO_DTC_CONFIGURATION_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WILCO_DTC_CONFIGURATION_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

namespace policy {

class PolicyService;

// This class observes the device setting "DeviceWilcoDtcConfiguration" and
// saves the wilco DTC (diagnostics and telemetry) controller configuration
// received in JSON format.
class DeviceWilcoDtcConfigurationHandler final
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  explicit DeviceWilcoDtcConfigurationHandler(PolicyService* policy_service);
  ~DeviceWilcoDtcConfigurationHandler() override;

  // DeviceCloudExternalDataPolicyObserver::Delegate:
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;

  void Shutdown();

 private:
  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_wilco_dtc_configuration_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceWilcoDtcConfigurationHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WILCO_DTC_CONFIGURATION_HANDLER_H_
