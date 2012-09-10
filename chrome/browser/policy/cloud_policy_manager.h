// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

class PrefService;

namespace policy {

class CloudPolicyClient;
class CloudPolicyRefreshScheduler;
class CloudPolicyService;

// CloudPolicyManager is the main switching central between cloud policy and the
// upper layers of the policy stack. It owns CloudPolicyStore,
// CloudPolicyClient, and CloudPolicyService, is responsible for receiving and
// keeping policy from the cloud and exposes the decoded policy via the
// ConfigurationPolicyProvider interface.
//
// This class contains the base functionality, there are subclasses that add
// functionality specific to user-level and device-level cloud policy, such as
// blocking on initial user policy fetch or device enrollment.
class CloudPolicyManager : public ConfigurationPolicyProvider,
                           public CloudPolicyStore::Observer {
 public:
  explicit CloudPolicyManager(scoped_ptr<CloudPolicyStore> store);
  virtual ~CloudPolicyManager();

  CloudPolicyClient* cloud_policy_client() { return client_.get(); }
  const CloudPolicyClient* cloud_policy_client() const { return client_.get(); }

  CloudPolicyStore* cloud_policy_store() { return store_.get(); }
  const CloudPolicyStore* cloud_policy_store() const { return store_.get(); }

  CloudPolicyService* cloud_policy_service() { return service_.get(); }
  const CloudPolicyService* cloud_policy_service() const {
    return service_.get();
  }

  // ConfigurationPolicyProvider:
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 protected:
  // Initializes the cloud connection.
  void InitializeService(scoped_ptr<CloudPolicyClient> client);

  // Shuts down the cloud connection.
  void ShutdownService();

  // Starts a refresh scheduler in case none is running yet. |local_state| must
  // stay valid until ShutdownService() gets called.
  void StartRefreshScheduler(PrefService* local_state,
                             const std::string& refresh_rate_pref);

  // Check whether fully initialized and if so, publish policy by calling
  // ConfigurationPolicyStore::UpdatePolicy().
  void CheckAndPublishPolicy();

 private:
  // Completion handler for policy refresh operations.
  void OnRefreshComplete();

  scoped_ptr<CloudPolicyStore> store_;
  scoped_ptr<CloudPolicyClient> client_;
  scoped_ptr<CloudPolicyService> service_;
  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;

  // Whether there's a policy refresh operation pending, in which case all
  // policy update notifications are deferred until after it completes.
  bool waiting_for_policy_refresh_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_
