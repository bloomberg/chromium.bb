// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"

namespace chromeos {
class SignedSettingsHelper;
}  // namespace chromeos

namespace policy {

class CloudPolicyDataStore;
class EnterpriseInstallAttributes;
class PolicyMap;

namespace em = enterprise_management;

// CloudPolicyCacheBase implementation that persists policy information
// to ChromeOS' session manager (via SignedSettingsHelper).
class DevicePolicyCache : public CloudPolicyCacheBase {
 public:
  DevicePolicyCache(CloudPolicyDataStore* data_store,
                    EnterpriseInstallAttributes* install_attributes);
  virtual ~DevicePolicyCache();

  // CloudPolicyCacheBase implementation:
  virtual void Load() OVERRIDE;
  virtual void SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;

  void OnRetrievePolicyCompleted(chromeos::SignedSettings::ReturnCode code,
                                 const em::PolicyFetchResponse& policy);

 private:
  friend class DevicePolicyCacheTest;
  friend class DevicePolicyCacheTestHelper;

  // Alternate c'tor allowing tests to mock out the SignedSettingsHelper
  // singleton.
  DevicePolicyCache(
      CloudPolicyDataStore* data_store,
      EnterpriseInstallAttributes* install_attributes,
      chromeos::SignedSettingsHelper* signed_settings_helper);

  // CloudPolicyCacheBase implementation:
  virtual bool DecodePolicyData(const em::PolicyData& policy_data,
                                PolicyMap* mandatory,
                                PolicyMap* recommended) OVERRIDE;

  void PolicyStoreOpCompleted(chromeos::SignedSettings::ReturnCode code);

  // Checks with immutable attributes whether this is an enterprise device and
  // read the registration user if this is the case.
  void CheckImmutableAttributes();

  // Tries to install the initial device policy retrieved from signed settings.
  // Fills in |device_token| if it could be extracted from the loaded protobuf.
  void InstallInitialPolicy(chromeos::SignedSettings::ReturnCode code,
                            const em::PolicyFetchResponse& policy,
                            std::string* device_token);

  static void DecodeDevicePolicy(const em::ChromeDeviceSettingsProto& policy,
                                 PolicyMap* mandatory,
                                 PolicyMap* recommended);

  CloudPolicyDataStore* data_store_;
  EnterpriseInstallAttributes* install_attributes_;

  chromeos::SignedSettingsHelper* signed_settings_helper_;

  base::WeakPtrFactory<DevicePolicyCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
