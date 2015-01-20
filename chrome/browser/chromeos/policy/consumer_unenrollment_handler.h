// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
class DeviceSettingsService;
class OwnerSettingsServiceChromeOS;
}

namespace policy {

class ConsumerManagementService;
class DeviceCloudPolicyManagerChromeOS;

// ConsumerUnenrollmentHandler unenrolls the device from the consumer management
// service. It sends a request to the server and resets the management settings.
class ConsumerUnenrollmentHandler : public KeyedService {
 public:
  ConsumerUnenrollmentHandler(
      chromeos::DeviceSettingsService* device_settings_service,
      ConsumerManagementService* consumer_management_service,
      DeviceCloudPolicyManagerChromeOS* device_cloud_policy_manager,
      chromeos::OwnerSettingsServiceChromeOS* owner_settings_service);
  ~ConsumerUnenrollmentHandler() override;

  // Starts the unenrollment process.
  void Start();

 private:
  void OnUnregistered(bool success);
  void OnManagementSettingsSet(bool success);

  chromeos::DeviceSettingsService* device_settings_service_;
  ConsumerManagementService* consumer_management_service_;
  DeviceCloudPolicyManagerChromeOS* device_cloud_policy_manager_;
  chromeos::OwnerSettingsServiceChromeOS* owner_settings_service_;

  base::WeakPtrFactory<ConsumerUnenrollmentHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerUnenrollmentHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_H_
