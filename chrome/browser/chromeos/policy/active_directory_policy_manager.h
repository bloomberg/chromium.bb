// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_scheduler.h"
#include "components/signin/core/account_id/account_id.h"

namespace policy {

// ConfigurationPolicyProvider for device or user policy from Active Directory.
// The choice of constructor determines whether device or user policy is
// provided.
// Data flow: Triggered by DoPolicyFetch(), policy is fetched by authpolicyd and
// stored in session manager with completion indicated by OnPolicyFetched().
// From there policy load from session manager is triggered, completion of which
// is notified via OnStoreLoaded()/OnStoreError().
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
      base::TimeDelta initial_policy_fetch_timeout,
      base::OnceClosure exit_session,
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
  PolicyScheduler* scheduler() { return scheduler_.get(); }

  // Helper function to force a policy fetch timeout.
  void ForceTimeoutForTest();

 private:
  // |account_id| specifies the user to manage policy for. If |account_id| is
  // empty, device policy is managed.
  //
  // The following applies to user policy only: If
  // |initial_policy_fetch_timeout| is non-zero, IsInitializationComplete() is
  // forced to false until either there has been a successful policy fetch from
  // the server and a subsequent successful load from session manager or
  // |initial_policy_fetch_timeout| has expired and there has been a successful
  // load from session manager. The timeout may be set to TimeDelta::Max() to
  // enforce successful policy fetch. In case the conditions for signaling
  // initialization complete are not met, the user session is aborted by calling
  // |exit_session|.
  ActiveDirectoryPolicyManager(const AccountId& account_id,
                               base::TimeDelta initial_policy_fetch_timeout,
                               base::OnceClosure exit_session,
                               std::unique_ptr<CloudPolicyStore> store);

  // Publish the policy that's currently cached in the store.
  void PublishPolicy();

  // Calls into authpolicyd to fetch policy. Reports success or failure via
  // |callback|.
  void DoPolicyFetch(PolicyScheduler::TaskCallback callback);

  // Called by scheduler with result of policy fetch. This covers policy
  // download, parsing and storing into session manager. (To access and publish
  // the policy, the store needs to be reloaded from session manager.)
  void OnPolicyFetched(bool success);

  // Called when |initial_policy_timeout_| times out, to cancel the blocking
  // wait for the initial policy fetch.
  void OnBlockingFetchTimeout();

  // Cancels waiting for the initial policy fetch/load and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed) or exits the session in case the requirements to continue
  // have not been met. |success| denotes whether the policy fetch was
  // successful.
  void CancelWaitForInitialPolicy(bool success);

  const AccountId account_id_;

  // Whether we're waiting for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool waiting_for_initial_policy_fetch_;

  // Whether the user session is continued in case of failure of initial policy
  // fetch.
  bool initial_policy_fetch_may_fail_;

  // Whether policy fetch has ever been reported as completed by authpolicyd.
  bool fetch_ever_completed_ = false;

  // Whether policy fetch has ever been reported as successful by authpolicyd.
  bool fetch_ever_succeeded_ = false;

  // A timer that puts a hard limit on the maximum time to wait for the initial
  // policy fetch/load.
  base::Timer initial_policy_timeout_{false /* retain_user_task */,
                                      false /* is_repeating */};

  // Callback to exit the session.
  base::OnceClosure exit_session_;

  std::unique_ptr<CloudPolicyStore> store_;
  std::unique_ptr<PolicyScheduler> scheduler_;

  // Must be last member.
  base::WeakPtrFactory<ActiveDirectoryPolicyManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
