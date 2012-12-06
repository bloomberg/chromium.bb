// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_local_account_policy_provider.h"

#include "base/bind.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

DeviceLocalAccountPolicyProvider::DeviceLocalAccountPolicyProvider(
    const std::string& account_id,
    DeviceLocalAccountPolicyService* service)
    : account_id_(account_id),
      service_(service),
      store_initialized_(false),
      waiting_for_policy_refresh_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  service_->AddObserver(this);
  UpdateFromBroker();
}

DeviceLocalAccountPolicyProvider::~DeviceLocalAccountPolicyProvider() {
  service_->RemoveObserver(this);
}

bool DeviceLocalAccountPolicyProvider::IsInitializationComplete() const {
  return ConfigurationPolicyProvider::IsInitializationComplete() &&
      store_initialized_;
}

void DeviceLocalAccountPolicyProvider::RefreshPolicies() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  if (broker) {
    waiting_for_policy_refresh_ = true;
    broker->RefreshPolicy(
        base::Bind(&DeviceLocalAccountPolicyProvider::ReportPolicyRefresh,
                   weak_factory_.GetWeakPtr()));
  } else {
    UpdateFromBroker();
  }
}

void DeviceLocalAccountPolicyProvider::OnPolicyUpdated(
    const std::string& account_id) {
  if (account_id == account_id_)
    UpdateFromBroker();
}

void DeviceLocalAccountPolicyProvider::OnDeviceLocalAccountsChanged() {
  UpdateFromBroker();
}

DeviceLocalAccountPolicyBroker* DeviceLocalAccountPolicyProvider::GetBroker() {
  return service_->GetBrokerForAccount(account_id_);
}

void DeviceLocalAccountPolicyProvider::ReportPolicyRefresh() {
  waiting_for_policy_refresh_ = false;
  UpdateFromBroker();
}

void DeviceLocalAccountPolicyProvider::UpdateFromBroker() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  if (broker) {
    store_initialized_ |= broker->store()->is_initialized();
    if (!waiting_for_policy_refresh_) {
      // Copy policy from the broker.
      bundle->Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(
          broker->store()->policy_map());
    } else {
      // Wait for the refresh to finish.
      return;
    }
  } else {
    // Keep existing policy, but do send an update.
    waiting_for_policy_refresh_ = false;
    weak_factory_.InvalidateWeakPtrs();
    bundle->CopyFrom(policies());
  }
  UpdatePolicy(bundle.Pass());
}

}  // namespace policy
