// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_provider.h"

#include "base/bind.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

AsynchronousPolicyProvider::AsynchronousPolicyProvider(
    const PolicyDefinitionList* policy_list,
    scoped_refptr<AsynchronousPolicyLoader> loader)
    : ConfigurationPolicyProvider(policy_list),
      loader_(loader) {
  loader_->Init(
      base::Bind(&AsynchronousPolicyProvider::NotifyPolicyUpdated,
                 base::Unretained(this)));
}

AsynchronousPolicyProvider::~AsynchronousPolicyProvider() {
  DCHECK(CalledOnValidThread());
  // |loader_| won't invoke its callback anymore after Stop(), therefore
  // Unretained(this) is safe in the ctor.
  loader_->Stop();
}

bool AsynchronousPolicyProvider::ProvideInternal(PolicyMap* map) {
  DCHECK(CalledOnValidThread());
  DCHECK(loader_->policy());
  map->LoadFrom(loader_->policy(), policy_definition_list());
  return true;
}

scoped_refptr<AsynchronousPolicyLoader> AsynchronousPolicyProvider::loader() {
  return loader_;
}

}  // namespace policy
