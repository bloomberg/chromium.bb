// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"

namespace policy {

// Class ConfigurationPolicyProvider.

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : policy_definition_list_(policy_list) {
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {}

bool ConfigurationPolicyProvider::IsInitializationComplete() const {
  return true;
}

void ConfigurationPolicyProvider::ApplyPolicyValueTree(
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

void ConfigurationPolicyProvider::ApplyPolicyMap(
    const PolicyMapType* policies,
    ConfigurationPolicyStoreInterface* store) {
  const PolicyDefinitionList* policy_list(policy_definition_list());
  for (const PolicyDefinitionList::Entry* i = policy_list->begin;
       i != policy_list->end; ++i) {
    PolicyMapType::const_iterator it =
        policies->find(i->policy_type);
    if (it != policies->end() && it->second->IsType(i->value_type))
      store->Apply(i->policy_type, it->second->DeepCopy());
  }
}

// Class ConfigurationPolicyObserverRegistrar.

ConfigurationPolicyObserverRegistrar::ConfigurationPolicyObserverRegistrar()
  : provider_(NULL),
    observer_(NULL) {}

ConfigurationPolicyObserverRegistrar::~ConfigurationPolicyObserverRegistrar() {
  if (provider_)
    provider_->RemoveObserver(this);
}

void ConfigurationPolicyObserverRegistrar::Init(
    ConfigurationPolicyProvider* provider,
    ConfigurationPolicyProvider::Observer* observer) {
  provider_ = provider;
  observer_ = observer;
  provider_->AddObserver(this);
}

void ConfigurationPolicyObserverRegistrar::OnUpdatePolicy() {
  observer_->OnUpdatePolicy();
}

void ConfigurationPolicyObserverRegistrar::OnProviderGoingAway() {
  observer_->OnProviderGoingAway();
  provider_->RemoveObserver(this);
  provider_ = NULL;
}

}  // namespace policy
