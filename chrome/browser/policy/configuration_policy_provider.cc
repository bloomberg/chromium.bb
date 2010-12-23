// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include <algorithm>

#include "base/values.h"

namespace policy {

ConfigurationPolicyObserverRegistrar::ConfigurationPolicyObserverRegistrar() {}

ConfigurationPolicyObserverRegistrar::~ConfigurationPolicyObserverRegistrar() {
  RemoveAll();
}

void ConfigurationPolicyObserverRegistrar::Init(
    ConfigurationPolicyProvider* provider) {
  provider_ = provider;
}

void ConfigurationPolicyObserverRegistrar::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observers_.push_back(observer);
  provider_->AddObserver(observer);
}

void ConfigurationPolicyObserverRegistrar::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  std::remove(observers_.begin(), observers_.end(), observer);
  provider_->RemoveObserver(observer);
}

void ConfigurationPolicyObserverRegistrar::RemoveAll() {
  for (std::vector<ConfigurationPolicyProvider::Observer*>::iterator it =
       observers_.begin(); it != observers_.end(); ++it) {
    provider_->RemoveObserver(*it);
  }
  observers_.clear();
}

// Class ConfigurationPolicyProvider.

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : policy_definition_list_(policy_list) {
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {}

void ConfigurationPolicyProvider::DecodePolicyValueTree(
    const DictionaryValue* policies,
    ConfigurationPolicyStoreInterface* store) {
  const PolicyDefinitionList* policy_list(policy_definition_list());
  for (const PolicyDefinitionList::Entry* i = policy_list->begin;
       i != policy_list->end; ++i) {
    Value* value;
    if (policies->Get(i->name, &value) && value->IsType(i->value_type))
      store->Apply(i->policy_type, value->DeepCopy());
  }

  // TODO(mnissler): Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}

}  // namespace policy
