// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_configuration_policy_provider.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "policy/policy_constants.h"

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider()
    : ConfigurationPolicyProvider(GetChromePolicyDefinitionList()) {}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy
