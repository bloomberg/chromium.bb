// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_manager.h"

class PrefService;
class Profile;

namespace policy {

class DeviceManagementService;

// UserCloudPolicyManager keeps track of all things user policy, drives the
// corresponding cloud policy service and publishes policy through the
// ConfigurationPolicyProvider interface.
class UserCloudPolicyManager : public CloudPolicyManager,
                               public CloudPolicyClient::Observer {
 public:
  // If |wait_for_policy| fetch is true, IsInitializationComplete() will return
  // false as long as there hasn't been a successful policy fetch.
  UserCloudPolicyManager(scoped_ptr<CloudPolicyStore> store,
                         bool wait_for_policy_fetch);
  virtual ~UserCloudPolicyManager();

  // Creates a UserCloudPolicyManager instance associated with the passed
  // |profile|.
  static scoped_ptr<UserCloudPolicyManager> Create(Profile* profile,
                                                   bool wait_for_policy_fetch);

  // Initializes the cloud connection. |local_state| and |service| must stay
  // valid until this object is deleted or ShutdownAndRemovePolicy() gets
  // called. Virtual for mocking.
  virtual void Initialize(PrefService* local_state,
                          DeviceManagementService* device_management_service,
                          UserAffiliation user_affiliation);

  // Shuts down the UserCloudPolicyManager (removes and stops refreshing the
  // cached cloud policy). This is typically called when a profile is being
  // disassociated from a given user (e.g. during signout). No policy will be
  // provided by this object until the next time Initialize() is invoked.
  void ShutdownAndRemovePolicy();

  // Cancels waiting for the policy fetch and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed).
  void CancelWaitForPolicyFetch();

  // Returns true if the underlying CloudPolicyClient is already registered.
  // Virtual for mocking.
  virtual bool IsClientRegistered() const;

  // Register the CloudPolicyClient using the passed OAuth token.
  void RegisterClient(const std::string& access_token);

  // ConfigurationPolicyProvider:
  virtual bool IsInitializationComplete() const OVERRIDE;

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

 private:
  // Completion handler for the explicit policy fetch triggered on startup in
  // case |wait_for_policy_fetch_| is true.
  void OnInitialPolicyFetchComplete();

  // Whether to wait for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool wait_for_policy_fetch_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_H_
