// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"

namespace {

using namespace policy;

bool IsProxyPolicy(ConfigurationPolicyType policy) {
  return policy == kPolicyProxyMode ||
         policy == kPolicyProxyServerMode ||
         policy == kPolicyProxyServer ||
         policy == kPolicyProxyPacUrl ||
         policy == kPolicyProxyBypassList;
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
