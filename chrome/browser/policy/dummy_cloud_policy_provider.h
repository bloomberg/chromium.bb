// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DUMMY_CLOUD_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_DUMMY_CLOUD_POLICY_PROVIDER_H_
#pragma once

#include "base/observer_list.h"
#include "chrome/browser/policy/cloud_policy_provider.h"

namespace policy {

// A cloud policy provider for tests, that provides 0 policies, but always
// reports that it is initialized.
class DummyCloudPolicyProvider : public CloudPolicyProvider {
 public:
  explicit DummyCloudPolicyProvider(const PolicyDefinitionList* policy_list);
  virtual ~DummyCloudPolicyProvider();

  // CloudPolicyProvider overrides:
  virtual void AppendCache(CloudPolicyCacheBase* cache) OVERRIDE;
  virtual void PrependCache(CloudPolicyCacheBase* cache) OVERRIDE;

  virtual bool Provide(PolicyMap* map);

 private:
  // ConfigurationPolicyProvider overrides:
  virtual void AddObserver(
      ConfigurationPolicyProvider::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      ConfigurationPolicyProvider::Observer* observer) OVERRIDE;

  ObserverList<ConfigurationPolicyProvider::Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DummyCloudPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DUMMY_CLOUD_POLICY_PROVIDER_H_
