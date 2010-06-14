// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_

#include "chrome/browser/configuration_policy_store.h"
#include "chrome/browser/configuration_policy_provider.h"

class DummyConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  DummyConfigurationPolicyProvider() {}
  virtual ~DummyConfigurationPolicyProvider() {}

  virtual bool Provide(ConfigurationPolicyStore* store) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyConfigurationPolicyProvider);
};

#endif  // CHROME_BROWSER_DUMMY_CONFIGURATION_POLICY_PROVIDER_H_

