// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mock_configuration_policy_provider.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/policy/core/common/policy_bundle.h"

using testing::Invoke;

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider() {}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {}

void MockConfigurationPolicyProvider::UpdateChromePolicy(
    const PolicyMap& policy) {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(policy);
  UpdatePolicy(bundle.Pass());
  if (base::MessageLoop::current())
    base::RunLoop().RunUntilIdle();
}

void MockConfigurationPolicyProvider::SetAutoRefresh() {
  EXPECT_CALL(*this, RefreshPolicies()).WillRepeatedly(
      Invoke(this, &MockConfigurationPolicyProvider::RefreshWithSamePolicies));
}

void MockConfigurationPolicyProvider::RefreshWithSamePolicies() {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(policies());
  UpdatePolicy(bundle.Pass());
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy
