// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

class BrowserPolicyConnector;

// A policy provider that merges the policies contained in the caches it
// observes. The caches receive their policies by fetching them from the cloud,
// through the CloudPolicyController.
class CloudPolicyProvider : public ConfigurationPolicyProvider,
                            public CloudPolicyCacheBase::Observer {
 public:
  CloudPolicyProvider(BrowserPolicyConnector* browser_policy_connector,
                      const PolicyDefinitionList* policy_list,
                      PolicyLevel level);
  virtual ~CloudPolicyProvider();

  // Sets the user policy cache. This must be invoked only once, and |cache|
  // must not be NULL.
  void SetUserPolicyCache(CloudPolicyCacheBase* cache);

#if defined(OS_CHROMEOS)
  // Sets the device policy cache. This must be invoked only once, and |cache|
  // must not be NULL.
  void SetDevicePolicyCache(CloudPolicyCacheBase* cache);
#endif

  // ConfigurationPolicyProvider implementation.
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // CloudPolicyCacheBase::Observer implementation.
  virtual void OnCacheUpdate(CloudPolicyCacheBase* cache) OVERRIDE;
  virtual void OnCacheGoingAway(CloudPolicyCacheBase* cache) OVERRIDE;

 private:
  // Indices of the known caches in |caches_|.
  enum {
    CACHE_USER,
#if defined(OS_CHROMEOS)
    CACHE_DEVICE,
#endif
    CACHE_SIZE,
  };

  // Merges policies from both caches, and triggers a notification if ready.
  void Merge();

  // The device and user policy caches, whose policies are provided by |this|.
  // Both are initially NULL, and the provider only becomes initialized once
  // all the caches are available and ready.
  CloudPolicyCacheBase* caches_[CACHE_SIZE];

  // Weak pointer to the connector. Guaranteed to outlive |this|.
  BrowserPolicyConnector* browser_policy_connector_;

  // The policy level published by this provider.
  PolicyLevel level_;

  // Whether all caches are present and fully initialized.
  bool initialization_complete_;

  // Used to determine when updates due to a RefreshPolicies() call have been
  // completed.
  std::set<const CloudPolicyCacheBase*> pending_updates_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
