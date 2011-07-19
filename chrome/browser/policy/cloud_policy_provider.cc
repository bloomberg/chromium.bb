// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_provider.h"

namespace policy {

CloudPolicyProvider::CloudPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list)
    : ConfigurationPolicyProvider(policy_list) {
}

CloudPolicyProvider::~CloudPolicyProvider() {
}

}  // namespace policy
