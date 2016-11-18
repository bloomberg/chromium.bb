// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_

#include <memory>

#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"

namespace policy {

// ConfigurationPolicyProvider for policy from Active Directory.  The policy is
// fetched from the Domain Controller by authpolicyd which stores it in session
// manager and from where it is loaded by DeviceActiveDirectoryPolicyManager.
// TODO(tnagel): Wire RefreshPolicies() through to authpolicyd.
class DeviceActiveDirectoryPolicyManager : public ConfigurationPolicyProvider,
                                           public CloudPolicyStore::Observer {
 public:
  explicit DeviceActiveDirectoryPolicyManager(
      std::unique_ptr<CloudPolicyStore> store);
  ~DeviceActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies() override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;
  void OnStoreError(CloudPolicyStore* cloud_policy_store) override;

  const CloudPolicyStore* store() const { return store_.get(); }

 private:
  // Publishes the policy that's currently cached in the store.
  void PublishPolicy();

  std::unique_ptr<CloudPolicyStore> store_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
