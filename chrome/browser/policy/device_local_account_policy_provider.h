// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_local_account_policy_service.h"

namespace policy {

// Policy provider for a device-local account. Pulls policy from
// DeviceLocalAccountPolicyService. Note that this implementation keeps
// functioning when the device-local account disappears from
// DeviceLocalAccountPolicyService. The current policy will be kept in that case
// and RefreshPolicies becomes a no-op.
class DeviceLocalAccountPolicyProvider
    : public ConfigurationPolicyProvider,
      public DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyProvider(const std::string& account_id,
                                   DeviceLocalAccountPolicyService* service);
  virtual ~DeviceLocalAccountPolicyProvider();

  // ConfigurationPolicyProvider:
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // DeviceLocalAccountPolicyService::Observer:
  virtual void OnPolicyUpdated(const std::string& account_id) OVERRIDE;
  virtual void OnDeviceLocalAccountsChanged() OVERRIDE;

 private:
  // Returns the broker for |account_id_| or NULL if not available.
  DeviceLocalAccountPolicyBroker* GetBroker();

  // Handles completion of policy refreshes and triggers the update callback.
  // |success| is true if the policy refresh was successful.
  void ReportPolicyRefresh(bool success);

  // Unless |waiting_for_policy_refresh_|, calls UpdatePolicy(), using the
  // policy from the broker if available or keeping the current policy.
  void UpdateFromBroker();

  const std::string account_id_;
  DeviceLocalAccountPolicyService* service_;

  bool store_initialized_;
  bool waiting_for_policy_refresh_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
