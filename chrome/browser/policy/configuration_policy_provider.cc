// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include <string>

#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

const char* kProxyPolicies[] = {
  key::kProxyMode,
  key::kProxyServerMode,
  key::kProxyServer,
  key::kProxyPacUrl,
  key::kProxyBypassList,
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
  result->CopyFrom(policy_bundle_.Get(POLICY_DOMAIN_CHROME, std::string()));
  return true;
}

bool ConfigurationPolicyProvider::IsInitializationComplete() const {
  return true;
}

// static
void ConfigurationPolicyProvider::FixDeprecatedPolicies(PolicyMap* policies) {
  // Proxy settings have been configured by 5 policies that didn't mix well
  // together, and maps of policies had to take this into account when merging
  // policy sources. The proxy settings will eventually be configured by a
  // single Dictionary policy when all providers have support for that. For
  // now, the individual policies are mapped here to a single Dictionary policy
  // that the rest of the policy machinery uses.

  // The highest (level, scope) pair for an existing proxy policy is determined
  // first, and then only policies with those exact attributes are merged.
  PolicyMap::Entry current_priority;  // Defaults to the lowest priority.
  scoped_ptr<DictionaryValue> proxy_settings(new DictionaryValue);
  for (size_t i = 0; i < arraysize(kProxyPolicies); ++i) {
    const PolicyMap::Entry* entry = policies->Get(kProxyPolicies[i]);
    if (entry) {
      if (entry->has_higher_priority_than(current_priority)) {
        proxy_settings->Clear();
        current_priority = *entry;
      }
      if (!entry->has_higher_priority_than(current_priority) &&
          !current_priority.has_higher_priority_than(*entry)) {
        proxy_settings->Set(kProxyPolicies[i], entry->value->DeepCopy());
      }
      policies->Erase(kProxyPolicies[i]);
    }
  }
  // Sets the new |proxy_settings| if kProxySettings isn't set yet, or if the
  // new priority is higher.
  const PolicyMap::Entry* existing = policies->Get(key::kProxySettings);
  if (!proxy_settings->empty() &&
      (!existing || current_priority.has_higher_priority_than(*existing))) {
    policies->Set(key::kProxySettings,
                  current_priority.level,
                  current_priority.scope,
                  proxy_settings.release());
  }
}

void ConfigurationPolicyProvider::UpdatePolicy(
    scoped_ptr<PolicyBundle> bundle) {
  policy_bundle_.Swap(bundle.get());
  FixDeprecatedPolicies(
      &policy_bundle_.Get(POLICY_DOMAIN_CHROME, std::string()));
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnUpdatePolicy(this));
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
  // Subtle: see the comment in OnProviderGoingAway().
  if (observer_)
    provider_->RemoveObserver(this);
}

void ConfigurationPolicyObserverRegistrar::Init(
    ConfigurationPolicyProvider* provider,
    ConfigurationPolicyProvider::Observer* observer) {
  // Must be either both NULL or both not NULL.
  DCHECK(provider);
  DCHECK(observer);
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
  // The |observer_| might delete |this| during this callback: don't touch any
  // of |this| field's after it returns.
  // It might also invoke provider() during this callback, so |provider_| can't
  // be set to NULL. So we set |observer_| to NULL instead to signal that
  // we're not observing the provider anymore.
  ConfigurationPolicyProvider::Observer* observer = observer_;
  observer_ = NULL;
  provider_->RemoveObserver(this);

  observer->OnProviderGoingAway(provider);
}

}  // namespace policy
