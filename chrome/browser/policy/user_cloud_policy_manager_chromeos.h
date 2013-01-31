// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_manager.h"

class PrefService;

namespace policy {

class DeviceManagementService;

// UserCloudPolicyManagerChromeOS implements logic for initializing user policy
// on Chrome OS.
class UserCloudPolicyManagerChromeOS : public CloudPolicyManager,
                                       public CloudPolicyClient::Observer {
 public:
  // If |wait_for_policy_fetch| is true, IsInitializationComplete() will return
  // false as long as there hasn't been a successful policy fetch.
  UserCloudPolicyManagerChromeOS(scoped_ptr<CloudPolicyStore> store,
                                 bool wait_for_policy_fetch);
  virtual ~UserCloudPolicyManagerChromeOS();

  // Initializes the cloud connection. |local_state| and
  // |device_management_service| must stay valid until this object is deleted.
  void Connect(PrefService* local_state,
               DeviceManagementService* device_management_service,
               UserAffiliation user_affiliation);

  // Cancels waiting for the policy fetch and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed).
  void CancelWaitForPolicyFetch();

  // Register the CloudPolicyClient using the passed OAuth token.
  void RegisterClient(const std::string& access_token);

  // Returns true if the underlying CloudPolicyClient is already registered.
  bool IsClientRegistered() const;

  // ConfigurationPolicyProvider:
  virtual void Shutdown() OVERRIDE;
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

 private:
  // Completion handler for the explicit policy fetch triggered on startup in
  // case |wait_for_policy_fetch_| is true. |success| is true if the fetch was
  // successful.
  void OnInitialPolicyFetchComplete(bool success);

  // Owns the store, note that CloudPolicyManager just keeps a plain pointer.
  scoped_ptr<CloudPolicyStore> store_;

  // Whether to wait for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool wait_for_policy_fetch_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
