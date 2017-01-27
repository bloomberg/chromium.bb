// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/signin/core/account_id/account_id.h"

namespace policy {

// ConfigurationPolicyProvider for device or user policy from Active Directory.
// The choice of constructor determines whether device or user policy is
// provided.  The policy is fetched from the Domain Controller by authpolicyd
// which stores it in session manager and from where it is loaded by
// ActiveDirectoryPolicyManager.
class ActiveDirectoryPolicyManager : public ConfigurationPolicyProvider,
                                     public CloudPolicyStore::Observer {
 public:
  ~ActiveDirectoryPolicyManager() override;

  // Create manager for device policy.
  static std::unique_ptr<ActiveDirectoryPolicyManager> CreateForDevicePolicy(
      std::unique_ptr<CloudPolicyStore> store);

  // Create manager for |accound_id| user policy.
  static std::unique_ptr<ActiveDirectoryPolicyManager> CreateForUserPolicy(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyStore> store);

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
  // |account_id| specifies the user to manage policy for.  If |account_id| is
  // empty, device policy is managed.
  ActiveDirectoryPolicyManager(const AccountId& account_id,
                               std::unique_ptr<CloudPolicyStore> store);

  // Publish the policy that's currently cached in the store.
  void PublishPolicy();

  // Callback from authpolicyd.
  void OnPolicyRefreshed(bool success);

  // Schedule next policy refresh to run after |delay|.  (Deletes any previously
  // scheduled refresh tasks.)
  void ScheduleRefresh(base::TimeDelta delay);

  // Schedule next automatic policy refresh based on initial fetch delay or
  // refresh interval.  (Deletes any previously scheduled refresh tasks.)
  void ScheduleAutomaticRefresh();

  // Actually execute the scheduled policy refresh.
  void RunScheduledRefresh();

  const AccountId account_id_;
  std::unique_ptr<CloudPolicyStore> store_;

  // Whether a policy refresh is in progress (and thus any further requests need
  // to be blocked).
  bool refresh_in_progress_ = false;
  // Whether a refresh had been blocked and thus the next refresh needs to be
  // scheduled at the shorter "retry" interval.
  bool retry_refresh_ = false;
  base::TimeTicks last_refresh_;
  const base::TimeTicks startup_ = base::TimeTicks::Now();
  std::unique_ptr<base::CancelableClosure> refresh_task_;

  // Must be last member.
  base::WeakPtrFactory<ActiveDirectoryPolicyManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
