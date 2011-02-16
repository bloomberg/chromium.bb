// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_provider.h"

#include "chrome/browser/policy/asynchronous_policy_loader.h"

namespace policy {

AsynchronousPolicyProvider::AsynchronousPolicyProvider(
    const PolicyDefinitionList* policy_list,
    scoped_refptr<AsynchronousPolicyLoader> loader)
    : ConfigurationPolicyProvider(policy_list),
      loader_(loader) {
  loader_->Init();
}

AsynchronousPolicyProvider::~AsynchronousPolicyProvider() {
  DCHECK(CalledOnValidThread());
  loader_->Stop();
}

bool AsynchronousPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  DCHECK(CalledOnValidThread());
  DCHECK(loader_->policy());
  ApplyPolicyValueTree(loader_->policy(), store);
  return true;
}

void AsynchronousPolicyProvider::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  loader_->AddObserver(observer);
}

void AsynchronousPolicyProvider::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  loader_->RemoveObserver(observer);
}

scoped_refptr<AsynchronousPolicyLoader> AsynchronousPolicyProvider::loader() {
  return loader_;
}

}  // namespace policy
