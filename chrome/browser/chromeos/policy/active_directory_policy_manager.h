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

class CloudExternalDataManager;

// ConfigurationPolicyProvider for policy from Active Directory.
// Derived classes implement specializations for user and device policy.
// Data flow: Triggered by DoPolicyFetch(), policy is fetched by authpolicyd and
// stored in session manager with completion indicated by OnPolicyFetched().
// From there policy load from session manager is triggered, completion of which
// is notified via OnStoreLoaded()/OnStoreError().
class ActiveDirectoryPolicyManager : public ConfigurationPolicyProvider,
                                     public CloudPolicyStore::Observer {
 public:
  ~ActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies() override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;
  void OnStoreError(CloudPolicyStore* cloud_policy_store) override;

  CloudPolicyStore* store() const { return store_.get(); }
  CloudExternalDataManager* external_data_manager() const {
    return external_data_manager_.get();
  }
  PolicyScheduler* scheduler() { return scheduler_.get(); }

 protected:
  ActiveDirectoryPolicyManager(
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager);

  // Publish the policy that's currently cached in the store.
  void PublishPolicy();

  // Calls into authpolicyd to fetch policy. Reports success or failure via
  // |callback|.
  virtual void DoPolicyFetch(PolicyScheduler::TaskCallback callback) = 0;

  // Allows derived classes to cancel waiting for the initial policy fetch/load
  // and to flag the ConfigurationPolicyProvider ready (assuming all other
  // initialization tasks have completed) or to exit the session in case the
  // requirements to continue have not been met. |success| denotes whether the
  // policy fetch was successful.
  virtual void CancelWaitForInitialPolicy(bool success) {}

 private:
  // Called by scheduler with result of policy fetch. This covers policy
  // download, parsing and storing into session manager. (To access and publish
  // the policy, the store needs to be reloaded from session manager.)
  void OnPolicyFetched(bool success);

  // Called right before policy is published. Expands e.g. ${machine_name} for
  // a selected set of policies.
  void ExpandVariables(PolicyMap* policy_map);

  // Whether policy fetch has ever been reported as completed by authpolicyd.
  bool fetch_ever_completed_ = false;

  // Whether policy fetch has ever been reported as successful by authpolicyd.
  bool fetch_ever_succeeded_ = false;

  // Store used to serialize policy, usually sends data to Session Manager.
  std::unique_ptr<CloudPolicyStore> store_;

  // Manages external data referenced by policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  std::unique_ptr<PolicyScheduler> scheduler_;

  // Must be last member.
  base::WeakPtrFactory<ActiveDirectoryPolicyManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryPolicyManager);
};

// Manages user policy for Active Directory managed devices.
class UserActiveDirectoryPolicyManager : public ActiveDirectoryPolicyManager {
 public:
  // If |initial_policy_fetch_timeout| is non-zero, IsInitializationComplete()
  // is forced to false until either there has been a successful policy fetch
  // from the server and a subsequent successful load from session manager or
  // |initial_policy_fetch_timeout| has expired and there has been a successful
  // load from session manager. The timeout may be set to TimeDelta::Max() to
  // enforce successful policy fetch. In case the conditions for signaling
  // initialization complete are not met, the user session is aborted by calling
  // |exit_session|.
  UserActiveDirectoryPolicyManager(
      const AccountId& account_id,
      base::TimeDelta initial_policy_fetch_timeout,
      base::OnceClosure exit_session,
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager);

  ~UserActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  bool IsInitializationComplete(PolicyDomain domain) const override;

  // Helper function to force a policy fetch timeout.
  void ForceTimeoutForTesting();

 protected:
  // ActiveDirectoryPolicyManager:
  void DoPolicyFetch(PolicyScheduler::TaskCallback callback) override;
  void CancelWaitForInitialPolicy(bool success) override;

 private:
  // Called when |initial_policy_timeout_| times out, to cancel the blocking
  // wait for the initial policy fetch.
  void OnBlockingFetchTimeout();

  // The user's account id.
  AccountId account_id_;

  // Whether we're waiting for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool waiting_for_initial_policy_fetch_ = false;

  // Whether the user session is continued in case of failure of initial policy
  // fetch.
  bool initial_policy_fetch_may_fail_ = false;

  // A timer that puts a hard limit on the maximum time to wait for the initial
  // policy fetch/load.
  base::Timer initial_policy_timeout_{false /* retain_user_task */,
                                      false /* is_repeating */};

  // Callback to exit the session.
  base::OnceClosure exit_session_;

  // Must be last member.
  base::WeakPtrFactory<UserActiveDirectoryPolicyManager> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(UserActiveDirectoryPolicyManager);
};

// Manages device policy for Active Directory managed devices.
class DeviceActiveDirectoryPolicyManager : public ActiveDirectoryPolicyManager {
 public:
  explicit DeviceActiveDirectoryPolicyManager(
      std::unique_ptr<CloudPolicyStore> store);
  ~DeviceActiveDirectoryPolicyManager() override;

 protected:
  // ActiveDirectoryPolicyManager:
  void DoPolicyFetch(PolicyScheduler::TaskCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceActiveDirectoryPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
