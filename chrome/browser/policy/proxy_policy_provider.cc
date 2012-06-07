// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/proxy_policy_provider.h"

namespace policy {

ProxyPolicyProvider::ProxyPolicyProvider() {}

ProxyPolicyProvider::~ProxyPolicyProvider() {}

void ProxyPolicyProvider::SetDelegate(ConfigurationPolicyProvider* delegate) {
  if (delegate) {
    registrar_.reset(new ConfigurationPolicyObserverRegistrar());
    registrar_->Init(delegate, this);
    OnUpdatePolicy(delegate);
  } else {
    registrar_.reset();
    UpdatePolicy(scoped_ptr<PolicyBundle>(new PolicyBundle()));
  }
}

void ProxyPolicyProvider::RefreshPolicies() {
  if (registrar_.get())
    registrar_->provider()->RefreshPolicies();
  else
    UpdatePolicy(scoped_ptr<PolicyBundle>(new PolicyBundle()));
}

void ProxyPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(registrar_->provider(), provider);
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->CopyFrom(registrar_->provider()->policies());
  UpdatePolicy(bundle.Pass());
}

void ProxyPolicyProvider::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {
  registrar_.reset();
  UpdatePolicy(scoped_ptr<PolicyBundle>(new PolicyBundle()));
}

}  // namespace policy
