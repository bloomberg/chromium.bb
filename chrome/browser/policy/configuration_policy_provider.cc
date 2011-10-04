// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "chrome/browser/policy/policy_map.h"

namespace policy {

// Class ConfigurationPolicyProvider.

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : policy_definition_list_(policy_list) {
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnProviderGoingAway());
}

bool ConfigurationPolicyProvider::Provide(PolicyMap* result) {
#if !defined(OFFICIAL_BUILD)
  if (override_policies_.get()) {
    result->CopyFrom(*override_policies_);
    return true;
  }
#endif
  return ProvideInternal(result);
}

bool ConfigurationPolicyProvider::IsInitializationComplete() const {
  return true;
}

#if !defined(OFFICIAL_BUILD)

void ConfigurationPolicyProvider::OverridePolicies(PolicyMap* policies) {
  override_policies_.reset(policies);
  NotifyPolicyUpdated();
}

#endif

void ConfigurationPolicyProvider::NotifyPolicyUpdated() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnUpdatePolicy());
}

void ConfigurationPolicyProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConfigurationPolicyProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
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
