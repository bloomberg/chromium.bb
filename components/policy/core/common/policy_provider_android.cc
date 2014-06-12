// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_provider_android.h"
#include "components/policy/core/common/policy_provider_android_delegate.h"

namespace policy {

namespace {

bool g_wait_for_policies = false;

}  // namespace

PolicyProviderAndroid::PolicyProviderAndroid()
    : delegate_(NULL),
      initialized_(!g_wait_for_policies) {}

PolicyProviderAndroid::~PolicyProviderAndroid() {}

const Schema* PolicyProviderAndroid::GetChromeSchema() const {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  return schema_map()->GetSchema(ns);
}

// static
void PolicyProviderAndroid::SetShouldWaitForPolicy(
    bool should_wait_for_policy) {
  g_wait_for_policies = should_wait_for_policy;
}

void PolicyProviderAndroid::SetDelegate(
    PolicyProviderAndroidDelegate* delegate) {
  delegate_ = delegate;
}

void PolicyProviderAndroid::SetPolicies(scoped_ptr<PolicyBundle> policy) {
  initialized_ = true;
  UpdatePolicy(policy.Pass());
}

void PolicyProviderAndroid::Shutdown() {
  if (delegate_)
    delegate_->PolicyProviderShutdown();

  ConfigurationPolicyProvider::Shutdown();
}

bool PolicyProviderAndroid::IsInitializationComplete(
    PolicyDomain domain) const {
  return initialized_;
}

void PolicyProviderAndroid::RefreshPolicies() {
  if (delegate_) {
    delegate_->RefreshPolicies();
  } else {
    // If we don't have a delegate, pass a copy of the current policies.
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->CopyFrom(policies());
    UpdatePolicy(bundle.Pass());
  }
}

}  // namespace policy
