// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_IMPL_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_IMPL_H_
#pragma once

#include <vector>

#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

class BrowserPolicyConnector;

class CloudPolicyProviderImpl : public CloudPolicyProvider,
                                public CloudPolicyCacheBase::Observer {
 public:
  CloudPolicyProviderImpl(BrowserPolicyConnector* browser_policy_connector,
                          const PolicyDefinitionList* policy_list,
                          CloudPolicyCacheBase::PolicyLevel level);
  virtual ~CloudPolicyProviderImpl();

  // ConfigurationPolicyProvider implementation.
  virtual bool ProvideInternal(PolicyMap* result) OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // CloudPolicyCacheBase::Observer implementation.
  virtual void OnCacheUpdate(CloudPolicyCacheBase* cache) OVERRIDE;
  virtual void OnCacheGoingAway(CloudPolicyCacheBase* cache) OVERRIDE;
  virtual void AppendCache(CloudPolicyCacheBase* cache) OVERRIDE;
  virtual void PrependCache(CloudPolicyCacheBase* cache) OVERRIDE;

 private:
  friend class CloudPolicyProviderTest;

  typedef std::vector<CloudPolicyCacheBase*> ListType;

  // Combines two PolicyMap and stores the result in out_map. The policies in
  // |base| take precedence over the policies in |overlay|. Proxy policies are
  // only applied in groups, that is if at least one proxy policy is present in
  // |base| then no proxy related policy of |overlay| will be applied.
  static void CombineTwoPolicyMaps(const PolicyMap& base,
                                   const PolicyMap& overlay,
                                   PolicyMap* out_map);

  // Recompute |combined_| from |caches_| and trigger an OnUpdatePolicy if
  // something changed. This is called whenever a change in one of the caches
  // is observed. For i=0..n-1: |caches_[i]| will contribute all its policies
  // except those already provided by |caches_[0]|..|caches_[i-1]|. Proxy
  // related policies are handled as a special case: they are only applied in
  // groups.
  void RecombineCachesAndTriggerUpdate();

  // Removes |cache| from |caches|, if contained therein.
  static void RemoveCache(CloudPolicyCacheBase* cache, ListType* caches);

  // Weak pointer to the connector. Guaranteed to outlive |this|.
  BrowserPolicyConnector* browser_policy_connector_;

  // The underlying policy caches.
  ListType caches_;

  // Caches with pending updates. Used by RefreshPolicies to determine if a
  // refresh has fully completed.
  ListType pending_update_caches_;

  // Policy level this provider will handle.
  CloudPolicyCacheBase::PolicyLevel level_;

  // Whether all caches are fully initialized.
  bool initialization_complete_;

  // The currently valid combination of all the maps in |caches_|. Will be
  // applied as is on call of Provide.
  PolicyMap combined_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyProviderImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_IMPL_H_
