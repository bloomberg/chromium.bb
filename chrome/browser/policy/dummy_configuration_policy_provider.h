// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include "base/observer_list.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

class ConfigurationPolicyStoreInterface;

class DummyConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  explicit DummyConfigurationPolicyProvider(
      const PolicyDefinitionList* policy_list);
  virtual ~DummyConfigurationPolicyProvider();

  virtual bool Provide(ConfigurationPolicyStoreInterface* store);

 private:
  // ConfigurationPolicyProvider overrides:
  virtual void AddObserver(ConfigurationPolicyProvider::Observer* observer);
  virtual void RemoveObserver(ConfigurationPolicyProvider::Observer* observer);

  ObserverList<ConfigurationPolicyProvider::Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DummyConfigurationPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_
