// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_provider.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"

namespace policy {

AsynchronousPolicyProvider::AsynchronousPolicyProvider(
    const PolicyDefinitionList* policy_list,
    scoped_refptr<AsynchronousPolicyLoader> loader)
    : ConfigurationPolicyProvider(policy_list),
      loader_(loader) {
  // TODO(danno): This explicit registration of the provider shouldn't be
  // necessary. Instead, the PrefStore should explicitly observe preference
  // changes that are reported during the policy change.
  loader_->SetProvider(this);
  loader_->Init();
}

AsynchronousPolicyProvider::~AsynchronousPolicyProvider() {
  DCHECK(CalledOnValidThread());
  loader_->Stop();
  loader_->SetProvider(NULL);
}

bool AsynchronousPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  DCHECK(CalledOnValidThread());
  DCHECK(loader_->policy());
  DecodePolicyValueTree(loader_->policy(), store);
  return true;
}

scoped_refptr<AsynchronousPolicyLoader> AsynchronousPolicyProvider::loader() {
  return loader_;
}

}  // namespace policy
