// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

ConfigurationPolicyType kProxyPolicies[] = {
  kPolicyProxyMode,
  kPolicyProxyServerMode,
  kPolicyProxyServer,
  kPolicyProxyPacUrl,
  kPolicyProxyBypassList,
};

}  // namespace

ConfigurationPolicyProvider::Observer::~Observer() {}

void ConfigurationPolicyProvider::Observer::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {}

// Class ConfigurationPolicyProvider.

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : policy_definition_list_(policy_list) {
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnProviderGoingAway(this));
}

bool ConfigurationPolicyProvider::Provide(PolicyMap* result) {
#if !defined(OFFICIAL_BUILD)
  if (override_policies_.get()) {
    result->CopyFrom(*override_policies_);
    return true;
  }
#endif
  if (ProvideInternal(result)) {
    FixDeprecatedPolicies(result);
    return true;
  }
  return false;
}

bool ConfigurationPolicyProvider::IsInitializationComplete() const {
  return true;
}

#if !defined(OFFICIAL_BUILD)

void ConfigurationPolicyProvider::OverridePolicies(PolicyMap* policies) {
  if (policies)
    FixDeprecatedPolicies(policies);
  override_policies_.reset(policies);
  NotifyPolicyUpdated();
}

#endif

void ConfigurationPolicyProvider::NotifyPolicyUpdated() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnUpdatePolicy(this));
}

// static
void ConfigurationPolicyProvider::FixDeprecatedPolicies(PolicyMap* policies) {
  // Proxy settings have been configured by 5 policies that didn't mix well
  // together, and maps of policies had to take this into account when merging
  // policy sources. The proxy settings will eventually be configured by a
  // single Dictionary policy when all providers have support for that. For
  // now, the individual policies are mapped here to a single Dictionary policy
  // that the rest of the policy machinery uses.
  scoped_ptr<DictionaryValue> proxy_settings(new DictionaryValue);
  for (size_t i = 0; i < arraysize(kProxyPolicies); ++i) {
    const Value* value = policies->Get(kProxyPolicies[i]);
    if (value) {
      proxy_settings->Set(GetPolicyName(kProxyPolicies[i]), value->DeepCopy());
      policies->Erase(kProxyPolicies[i]);
    }
  }
  if (!proxy_settings->empty() && !policies->Get(kPolicyProxySettings))
    policies->Set(kPolicyProxySettings, proxy_settings.release());
}

void ConfigurationPolicyProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConfigurationPolicyProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

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

void ConfigurationPolicyObserverRegistrar::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(provider_, provider);
  observer_->OnUpdatePolicy(provider_);
}

void ConfigurationPolicyObserverRegistrar::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(provider_, provider);
  observer_->OnProviderGoingAway(provider_);
  provider_->RemoveObserver(this);
  provider_ = NULL;
}

}  // namespace policy
