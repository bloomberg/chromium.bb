// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class PrefService;
class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class CloudExternalDataManager;
class DeviceManagementService;
class UserCloudPolicyStore;

// UserCloudPolicyManager handles initialization of user policy for Chrome
// Profiles on the desktop platforms.
class UserCloudPolicyManager : public CloudPolicyManager,
                               public BrowserContextKeyedService {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  UserCloudPolicyManager(
      Profile* profile,
      scoped_ptr<UserCloudPolicyStore> store,
      scoped_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~UserCloudPolicyManager();

  virtual void Shutdown() OVERRIDE;

  void SetSigninUsername(const std::string& username);

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted or DisconnectAndRemovePolicy() gets called. Virtual for
  // mocking.
  virtual void Connect(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      scoped_ptr<CloudPolicyClient> client);

  // Shuts down the UserCloudPolicyManager (removes and stops refreshing the
  // cached cloud policy). This is typically called when a profile is being
  // disassociated from a given user (e.g. during signout). No policy will be
  // provided by this object until the next time Initialize() is invoked.
  void DisconnectAndRemovePolicy();

  // Returns true if the underlying CloudPolicyClient is already registered.
  // Virtual for mocking.
  virtual bool IsClientRegistered() const;

  // Creates a CloudPolicyClient for this client. Used in situations where
  // callers want to create a DMToken without actually initializing the
  // profile's policy infrastructure.
  static scoped_ptr<CloudPolicyClient> CreateCloudPolicyClient(
      DeviceManagementService* device_management_service);

 private:
  // The profile this instance belongs to.
  Profile* profile_;

  // Typed pointer to the store owned by UserCloudPolicyManager. Note that
  // CloudPolicyManager only keeps a plain CloudPolicyStore pointer.
  scoped_ptr<UserCloudPolicyStore> store_;

  // Manages external data referenced by policies.
  scoped_ptr<CloudExternalDataManager> external_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
