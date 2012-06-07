// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

class PrefService;

namespace policy {

class CloudPolicyRefreshScheduler;
class CloudPolicyService;
class DeviceManagementService;

// UserCloudPolicyManager keeps track of all things user policy, drives the
// corresponding cloud policy service and publishes policy through the
// ConfigurationPolicyProvider interface.
class UserCloudPolicyManager : public ConfigurationPolicyProvider,
                               public CloudPolicyClient::Observer,
                               public CloudPolicyStore::Observer {
 public:
  // If |wait_for_policy| fetch is true, IsInitializationComplete() will return
  // false as long as there hasn't been a successful policy fetch.
  UserCloudPolicyManager(scoped_ptr<CloudPolicyStore> store,
                         bool wait_for_policy_fetch);
  virtual ~UserCloudPolicyManager();

#if defined(OS_CHROMEOS)
  // Creates a UserCloudPolicyService instance for the Chrome OS platform.
  static scoped_ptr<UserCloudPolicyManager> Create(bool wait_for_policy_fetch);
#endif

  // Initializes the cloud connection. |prefs| and |service| must stay valid
  // until Shutdown() gets called.
  void Initialize(PrefService* prefs,
                  DeviceManagementService* service,
                  UserAffiliation user_affiliation);
  void Shutdown();

  // Cancels waiting for the policy fetch and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed).
  void CancelWaitForPolicyFetch();

  CloudPolicyService* cloud_policy_service() { return service_.get(); }

  // ConfigurationPolicyProvider:
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  // Check whether fully initialized and if so, publish policy by calling
  // ConfigurationPolicyStore::UpdatePolicy().
  void CheckAndPublishPolicy();

  // Completion handler for the explicit policy fetch triggered on startup in
  // case |wait_for_policy_fetch_| is true.
  void OnInitialPolicyFetchComplete();

  // Completion handler for policy refresh operations.
  void OnRefreshComplete();

  // Whether to wait for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool wait_for_policy_fetch_;

  // Whether there's a policy refresh operation pending, in which case all
  // policy update notifications are deferred until after it completes.
  bool wait_for_policy_refresh_;

  scoped_ptr<CloudPolicyStore> store_;
  scoped_ptr<CloudPolicyService> service_;
  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_
