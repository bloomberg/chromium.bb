// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

class CloudPolicyCacheBase;

// A policy provider having multiple backend caches, combining their relevant
// PolicyMaps and keeping the result cached. The underlying caches are kept as
// weak references and can be added  dynamically. Also the
// |CloudPolicyProvider| instance listens to cache-notifications and removes
// the caches automatically when they go away. The order in which the caches are
// stored matters! The first cache is applied as is and the following caches
// only contribute the not-yet applied policies. There are two functions to add
// a new cache:
//   PrependCache(cache): adds |cache| to the front (i.e. most important cache).
//   AppendCache(cache):  adds |cache| to the back (i.e. least important cache).
class CloudPolicyProvider : public ConfigurationPolicyProvider {
 public:
  explicit CloudPolicyProvider(
      const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list);
  virtual ~CloudPolicyProvider();

  // Adds a new instance of CloudPolicyCacheBase to the end of |caches_|.
  // Does not take ownership of |cache| and listens to OnCacheGoingAway to
  // automatically remove it from |caches_|.
  virtual void AppendCache(CloudPolicyCacheBase* cache) = 0;

  // Adds a new instance of CloudPolicyCacheBase to the beginning of |caches_|.
  // Does not take ownership of |cache| and listens to OnCacheGoingAway to
  // automatically remove it from |caches_|.
  virtual void PrependCache(CloudPolicyCacheBase* cache) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_PROVIDER_H_
