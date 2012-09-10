// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_STORE_CHROMEOS_H_
#define CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_STORE_CHROMEOS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/cloud_policy_validator.h"

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace policy {

class EnterpriseInstallAttributes;

// CloudPolicyStore implementation for device policy on Chrome OS. Policy is
// stored/loaded via DBus to/from session_manager.
class DeviceCloudPolicyStoreChromeOS
    : public CloudPolicyStore,
      public chromeos::DeviceSettingsService::Observer {
 public:
  DeviceCloudPolicyStoreChromeOS(
      chromeos::DeviceSettingsService* device_settings_service,
      EnterpriseInstallAttributes* install_attributes);
  virtual ~DeviceCloudPolicyStoreChromeOS();

  // CloudPolicyStore:
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void Load() OVERRIDE;
  virtual void RemoveStoredPolicy() OVERRIDE;

  // Installs initial policy. This is different from Store() in that it skips
  // the signature validation step against already-installed policy. The checks
  // against installation-time attributes are performed nevertheless. The result
  // of the operation is reported through the OnStoreLoaded() or OnStoreError()
  // observer callbacks.
  void InstallInitialPolicy(
      const enterprise_management::PolicyFetchResponse& policy);

  // chromeos::DeviceSettingsService::Observer:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

 private:
  // Create a validator for |policy| with basic device policy configuration and
  // OnPolicyStored() as the completion callback.
  scoped_ptr<DeviceCloudPolicyValidator> CreateValidator(
      const enterprise_management::PolicyFetchResponse& policy);

  // Called on completion on the policy validation prior to storing policy.
  // Starts the actual store operation.
  void OnPolicyToStoreValidated(DeviceCloudPolicyValidator* validator);

  // Handles store completion operations updates status.
  void OnPolicyStored();

  // Re-syncs policy and status from |device_settings_service_|.
  void UpdateFromService();

  chromeos::DeviceSettingsService* device_settings_service_;
  EnterpriseInstallAttributes* install_attributes_;

  base::WeakPtrFactory<DeviceCloudPolicyStoreChromeOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyStoreChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_CLOUD_POLICY_STORE_CHROMEOS_H_
