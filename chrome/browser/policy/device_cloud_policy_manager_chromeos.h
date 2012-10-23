// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud_policy_manager.h"
#include "chrome/browser/policy/cloud_policy_store.h"

class PrefService;

namespace policy {

class CloudPolicyClient;
class DeviceCloudPolicyStoreChromeOS;
class DeviceManagementService;
class EnterpriseInstallAttributes;

// CloudPolicyManager specialization for device policy on Chrome OS.
class DeviceCloudPolicyManagerChromeOS : public CloudPolicyManager {
 public:
  DeviceCloudPolicyManagerChromeOS(
      scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
      EnterpriseInstallAttributes* install_attributes);
  virtual ~DeviceCloudPolicyManagerChromeOS();

  // Establishes the connection to the cloud, updating policy as necessary.
  void Connect(PrefService* local_state,
               DeviceManagementService* device_management_service);

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;

  // Returns the device serial number, or an empty string if not available.
  static std::string GetMachineID();

  // Returns the machine model, or an empty string if not available.
  static std::string GetMachineModel();

 private:
  // Creates a new CloudPolicyClient.
  scoped_ptr<CloudPolicyClient> CreateClient();

  // Starts policy refreshes if |store_| indicates a managed device and the
  // necessary dependencies have been provided via Initialize().
  void StartIfManaged();

  // Points to the same object as the base CloudPolicyManager::store(), but with
  // actual device policy specific type.
  DeviceCloudPolicyStoreChromeOS* device_store_;
  EnterpriseInstallAttributes* install_attributes_;

  DeviceManagementService* device_management_service_;

  // PrefService instance to read the policy refresh rate from.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
