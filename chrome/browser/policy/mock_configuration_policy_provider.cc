// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_configuration_policy_provider.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "policy/policy_constants.h"

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider()
    : ConfigurationPolicyProvider(GetChromePolicyDefinitionList()) {}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {}

void MockConfigurationPolicyProvider::UpdateChromePolicy(
    const PolicyMap& policy) {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(policy);
  UpdatePolicy(bundle.Pass());
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy
