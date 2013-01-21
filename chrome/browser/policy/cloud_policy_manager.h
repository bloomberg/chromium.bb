// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_core.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

// CloudPolicyManager is the main switching central between cloud policy and the
// upper layers of the policy stack. It wires up a CloudPolicyCore to the
// ConfigurationPolicyProvider interface.
//
// This class contains the base functionality, there are subclasses that add
// functionality specific to user-level and device-level cloud policy, such as
// blocking on initial user policy fetch or device enrollment.
class CloudPolicyManager : public ConfigurationPolicyProvider,
                           public CloudPolicyStore::Observer {
 public:
  CloudPolicyManager(const PolicyNamespaceKey& policy_ns_key,
                     CloudPolicyStore* cloud_policy_store);
  virtual ~CloudPolicyManager();

  CloudPolicyCore* core() { return &core_; }
  const CloudPolicyCore* core() const { return &core_; }

  // ConfigurationPolicyProvider:
  virtual void Shutdown() OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* cloud_policy_store) OVERRIDE;

 protected:
  // Check whether fully initialized and if so, publish policy by calling
  // ConfigurationPolicyStore::UpdatePolicy().
  void CheckAndPublishPolicy();

  // Convenience accessors to core() components.
  CloudPolicyClient* client() { return core_.client(); }
  const CloudPolicyClient* client() const { return core_.client(); }
  CloudPolicyStore* store() { return core_.store(); }
  const CloudPolicyStore* store() const { return core_.store(); }
  CloudPolicyService* service() { return core_.service(); }
  const CloudPolicyService* service() const { return core_.service(); }

 private:
  // Completion handler for policy refresh operations.
  void OnRefreshComplete(bool success);

  CloudPolicyCore core_;

  // Whether there's a policy refresh operation pending, in which case all
  // policy update notifications are deferred until after it completes.
  bool waiting_for_policy_refresh_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_MANAGER_H_
