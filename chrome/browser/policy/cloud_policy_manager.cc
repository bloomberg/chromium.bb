// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_service.h"

namespace policy {

CloudPolicyManager::CloudPolicyManager(CloudPolicyStore* cloud_policy_store)
    : core_(cloud_policy_store),
      waiting_for_policy_refresh_(false) {
  store()->AddObserver(this);

  // If the underlying store is already initialized, publish the loaded
  // policy. Otherwise, request a load now.
  if (store()->is_initialized())
    CheckAndPublishPolicy();
  else
    store()->Load();
}

CloudPolicyManager::~CloudPolicyManager() {}

void CloudPolicyManager::Shutdown() {
  core_.Disconnect();
  store()->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool CloudPolicyManager::IsInitializationComplete() const {
  return store()->is_initialized();
}

void CloudPolicyManager::RefreshPolicies() {
  if (service()) {
    waiting_for_policy_refresh_ = true;
    service()->RefreshPolicy(
        base::Bind(&CloudPolicyManager::OnRefreshComplete,
                   base::Unretained(this)));
  } else {
    OnRefreshComplete(false);
  }
}

void CloudPolicyManager::OnStoreLoaded(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnStoreError(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this
  // is only required on the first load, but doesn't hurt in any case.
  CheckAndPublishPolicy();
}

void CloudPolicyManager::CheckAndPublishPolicy() {
  if (IsInitializationComplete() && !waiting_for_policy_refresh_) {
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(
        store()->policy_map());
    UpdatePolicy(bundle.Pass());
  }
}

void CloudPolicyManager::OnRefreshComplete(bool success) {
  waiting_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
