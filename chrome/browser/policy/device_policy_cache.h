// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// CloudPolicyCacheBase implementation that persists policy information
// to ChromeOS' session manager (via SignedSettingsHelper).
class DevicePolicyCache : public CloudPolicyCacheBase {
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

  void OnRetrievePolicyCompleted(
      chromeos::SignedSettings::ReturnCode code,
      const enterprise_management::PolicyFetchResponse& policy);

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
  virtual bool DecodePolicyData(
      const enterprise_management::PolicyData& policy_data,
      PolicyMap* policies) OVERRIDE;

  void PolicyStoreOpCompleted(chromeos::SignedSettings::ReturnCode code);

  // Checks with immutable attributes whether this is an enterprise device and
  // read the registration user if this is the case.
  void CheckImmutableAttributes();

  // Tries to install the initial device policy retrieved from signed settings.
  // Fills in |device_token| if it could be extracted from the loaded protobuf.
  void InstallInitialPolicy(
      chromeos::SignedSettings::ReturnCode code,
      const enterprise_management::PolicyFetchResponse& policy,
      std::string* device_token);

  // Ensures that CrosSettings has established trust on the reporting prefs and
  // publishes the |device_token| loaded from the cache. It's important that we
  // have fully-initialized device settings s.t. device status uploads get the
  // correct reporting policy flags.
  void SetTokenAndFlagReady(const std::string& device_token);

  // Checks whether a policy fetch is pending and sends out a notification if
  // that is the case.
  void CheckFetchingDone();

  void DecodeDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies);

  // Decode the various groups of policies.
  static void DecodeLoginPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies);
  static void DecodeKioskPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies,
      EnterpriseInstallAttributes* install_attributes);
  static void DecodeNetworkPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies,
      EnterpriseInstallAttributes* install_attributes);
  static void DecodeReportingPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies);
  static void DecodeGenericPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyMap* policies);

  CloudPolicyDataStore* data_store_;
  EnterpriseInstallAttributes* install_attributes_;

  chromeos::SignedSettingsHelper* signed_settings_helper_;

  base::WeakPtrFactory<DevicePolicyCache> weak_ptr_factory_;

  bool policy_fetch_pending_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
