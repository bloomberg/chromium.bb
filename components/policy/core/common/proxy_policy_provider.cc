// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/proxy_policy_provider.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/policy_bundle.h"

namespace policy {

ProxyPolicyProvider::ProxyPolicyProvider() : delegate_(NULL) {}

ProxyPolicyProvider::~ProxyPolicyProvider() {
  DCHECK(!delegate_);
}

void ProxyPolicyProvider::SetDelegate(ConfigurationPolicyProvider* delegate) {
  if (delegate_)
    delegate_->RemoveObserver(this);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->AddObserver(this);
    OnUpdatePolicy(delegate_);
  } else {
    UpdatePolicy(scoped_ptr<PolicyBundle>(new PolicyBundle()));
  }
}

void ProxyPolicyProvider::Shutdown() {
  // Note: the delegate is not owned by the proxy provider, so this call is not
  // forwarded. The same applies for the Init() call.
  // Just drop the delegate without propagating updates here.
  if (delegate_) {
    delegate_->RemoveObserver(this);
    delegate_ = NULL;
  }
  ConfigurationPolicyProvider::Shutdown();
}

void ProxyPolicyProvider::RefreshPolicies() {
  if (delegate_) {
    delegate_->RefreshPolicies();
  } else {
    // Subtle: if a RefreshPolicies() call comes after Shutdown() then the
    // current bundle should be served instead. This also does the right thing
    // if SetDelegate() was never called before.
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->CopyFrom(policies());
    UpdatePolicy(bundle.Pass());
  }
}

void ProxyPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(delegate_, provider);
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->CopyFrom(delegate_->policies());
  UpdatePolicy(bundle.Pass());
}

}  // namespace policy
