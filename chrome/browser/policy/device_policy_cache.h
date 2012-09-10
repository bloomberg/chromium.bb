// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"

namespace policy {

class CloudPolicyDataStore;
class EnterpriseInstallAttributes;
class PolicyMap;

// CloudPolicyCacheBase implementation that persists policy information
// to ChromeOS' session manager (via DeviceSettingsService).
class DevicePolicyCache : public CloudPolicyCacheBase,
                          public chromeos::DeviceSettingsService::Observer {
 public:
  DevicePolicyCache(CloudPolicyDataStore* data_store,
                    EnterpriseInstallAttributes* install_attributes);
  virtual ~DevicePolicyCache();

  // CloudPolicyCacheBase implementation:
  virtual void Load() OVERRIDE;
  virtual bool SetPolicy(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;
  virtual void SetFetchingDone() OVERRIDE;

  // DeviceSettingsService::Observer implementation:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

 private:
  friend class DevicePolicyCacheTest;
  friend class DevicePolicyCacheTestHelper;

  // Alternate c'tor allowing tests to mock out the DeviceSettingsService
  // singleton.
  DevicePolicyCache(
      CloudPolicyDataStore* data_store,
      EnterpriseInstallAttributes* install_attributes,
      chromeos::DeviceSettingsService* device_settings_service);

  // CloudPolicyCacheBase implementation:
  virtual bool DecodePolicyData(
      const enterprise_management::PolicyData& policy_data,
      PolicyMap* policies) OVERRIDE;

  // Handles completion of policy store operations.
  void PolicyStoreOpCompleted();

  // Checks with immutable attributes whether this is an enterprise device and
  // read the registration user if this is the case.
  void CheckImmutableAttributes();

  // Tries to install the initial device policy retrieved from signed settings.
  // Fills in |device_token| if it could be extracted from the loaded protobuf.
  void InstallInitialPolicy(
      chromeos::DeviceSettingsService::Status status,
      const enterprise_management::PolicyData* policy_data,
      std::string* device_token);

  // Ensures that CrosSettings has established trust on the reporting prefs and
  // publishes the |device_token| loaded from the cache. It's important that we
  // have fully-initialized device settings s.t. device status uploads get the
  // correct reporting policy flags.
  void SetTokenAndFlagReady(const std::string& device_token);

  // Checks whether a policy fetch is pending and sends out a notification if
  // that is the case.
  void CheckFetchingDone();

  CloudPolicyDataStore* data_store_;
  EnterpriseInstallAttributes* install_attributes_;

  chromeos::DeviceSettingsService* device_settings_service_;

  base::WeakPtrFactory<DevicePolicyCache> weak_ptr_factory_;

  bool policy_fetch_pending_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
