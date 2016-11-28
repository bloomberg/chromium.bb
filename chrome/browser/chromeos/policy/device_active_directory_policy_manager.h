// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"

namespace policy {

// ConfigurationPolicyProvider for policy from Active Directory.  The policy is
// fetched from the Domain Controller by authpolicyd which stores it in session
// manager and from where it is loaded by DeviceActiveDirectoryPolicyManager.
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

  CloudPolicyStore* store() const { return store_.get(); }

 private:
  // Publishes the policy that's currently cached in the store.
  void PublishPolicy();

  // Callback from authpolicyd.
  void OnPolicyRefreshed(bool success);

  std::unique_ptr<CloudPolicyStore> store_;

  // Must be last member.
  base::WeakPtrFactory<DeviceActiveDirectoryPolicyManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceActiveDirectoryPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
