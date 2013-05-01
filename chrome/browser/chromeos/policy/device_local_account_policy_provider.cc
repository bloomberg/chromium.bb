// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"

#include "base/bind.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
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
      weak_factory_(this) {
  service_->AddObserver(this);
  UpdateFromBroker();
}

DeviceLocalAccountPolicyProvider::~DeviceLocalAccountPolicyProvider() {
  service_->RemoveObserver(this);
}

bool DeviceLocalAccountPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_initialized_;
  return true;
}

void DeviceLocalAccountPolicyProvider::RefreshPolicies() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  if (broker && broker->core()->service()) {
    waiting_for_policy_refresh_ = true;
    broker->core()->service()->RefreshPolicy(
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

void DeviceLocalAccountPolicyProvider::ReportPolicyRefresh(bool success) {
  waiting_for_policy_refresh_ = false;
  UpdateFromBroker();
}

void DeviceLocalAccountPolicyProvider::UpdateFromBroker() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  if (broker) {
    store_initialized_ |= broker->core()->store()->is_initialized();
    if (!waiting_for_policy_refresh_) {
      // Copy policy from the broker.
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .CopyFrom(broker->core()->store()->policy_map());
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
