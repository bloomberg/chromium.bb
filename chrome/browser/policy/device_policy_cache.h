// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"

namespace policy {

class PolicyMap;

namespace em = enterprise_management;

// CloudPolicyCacheBase implementation that persists policy information
// to ChromeOS' session manager (via SignedSettingsHelper).
class DevicePolicyCache : public CloudPolicyCacheBase,
                          public chromeos::SignedSettingsHelper::Callback {
 public:
  DevicePolicyCache();
  virtual ~DevicePolicyCache();

  // CloudPolicyCacheBase implementation:
  virtual void Load() OVERRIDE;
  virtual void SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;

  // SignedSettingsHelper::Callback implementation:
  virtual void OnStorePolicyCompleted(
      chromeos::SignedSettings::ReturnCode code) OVERRIDE;
  virtual void OnRetrievePolicyCompleted(
      chromeos::SignedSettings::ReturnCode code,
      const em::PolicyFetchResponse& policy) OVERRIDE;

 private:
  friend class DevicePolicyCacheTest;

  // Alternate c'tor allowing tests to mock out the SignedSettingsHelper
  // singleton.
  explicit DevicePolicyCache(
      chromeos::SignedSettingsHelper* signed_settings_helper);

  // CloudPolicyCacheBase implementation:
  virtual bool DecodePolicyData(const em::PolicyData& policy_data,
                                PolicyMap* mandatory,
                                PolicyMap* recommended) OVERRIDE;

  static void DecodeDevicePolicy(const em::ChromeDeviceSettingsProto& policy,
                                 PolicyMap* mandatory,
                                 PolicyMap* recommended);

  chromeos::SignedSettingsHelper* signed_settings_helper_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_CACHE_H_
