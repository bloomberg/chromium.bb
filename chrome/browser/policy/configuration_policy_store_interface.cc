// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_store_interface.h"

#include "base/values.h"

namespace {

bool IsProxyPolicy(policy::ConfigurationPolicyType policy) {
  return policy == policy::kPolicyProxyMode ||
         policy == policy::kPolicyProxyServerMode ||
         policy == policy::kPolicyProxyServer ||
         policy == policy::kPolicyProxyPacUrl ||
         policy == policy::kPolicyProxyBypassList;
}

} // namespace

namespace policy {

void ObservingPolicyStoreInterface::Apply(ConfigurationPolicyType policy,
                                          Value* value) {
  next_->Apply(policy, value);

  if (IsProxyPolicy(policy))
      proxy_policy_applied_ = true;
}

void FilteringPolicyStoreInterface::Apply(ConfigurationPolicyType policy,
                                          Value* value) {
  // Apply() takes ownership of |value|.
  if (IsProxyPolicy(policy) && apply_proxy_policies_)
    next_->Apply(policy, value);
  else
    delete value;
}

} // namespace policy
