// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/configuration_policy_provider.h"

#include "base/callback.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

ConfigurationPolicyProvider::Observer::~Observer() {}

ConfigurationPolicyProvider::ConfigurationPolicyProvider()
    : did_shutdown_(false),
      schema_registry_(NULL) {}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {
  DCHECK(did_shutdown_);
}

void ConfigurationPolicyProvider::Init(SchemaRegistry* registry) {
  schema_registry_ = registry;
  schema_registry_->AddObserver(this);
}

void ConfigurationPolicyProvider::Shutdown() {
  did_shutdown_ = true;
  if (schema_registry_) {
    // Unit tests don't initialize the BrowserPolicyConnector but call
    // shutdown; handle that.
    schema_registry_->RemoveObserver(this);
    schema_registry_ = NULL;
  }
}

bool ConfigurationPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  return true;
}

void ConfigurationPolicyProvider::UpdatePolicy(
    scoped_ptr<PolicyBundle> bundle) {
  if (bundle.get())
    policy_bundle_.Swap(bundle.get());
  else
    policy_bundle_.Clear();
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnUpdatePolicy(this));
}

SchemaRegistry* ConfigurationPolicyProvider::schema_registry() const {
  return schema_registry_;
}

const scoped_refptr<SchemaMap>&
ConfigurationPolicyProvider::schema_map() const {
  return schema_registry_->schema_map();
}

void ConfigurationPolicyProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConfigurationPolicyProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConfigurationPolicyProvider::OnSchemaRegistryUpdated(
    bool has_new_schemas) {}

void ConfigurationPolicyProvider::OnSchemaRegistryReady() {}

}  // namespace policy
