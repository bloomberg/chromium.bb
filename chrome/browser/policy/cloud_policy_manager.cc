// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

CloudPolicyManager::CloudPolicyManager(scoped_ptr<CloudPolicyStore> store)
    : store_(store.Pass()),
      waiting_for_policy_refresh_(false) {
  store_->AddObserver(this);
  store_->Load();
}

CloudPolicyManager::~CloudPolicyManager() {
  ShutdownService();
  store_->RemoveObserver(this);
}

bool CloudPolicyManager::IsInitializationComplete() const {
  return store_->is_initialized();
}

void CloudPolicyManager::RefreshPolicies() {
  if (service_.get()) {
    waiting_for_policy_refresh_ = true;
    service_->RefreshPolicy(
        base::Bind(&CloudPolicyManager::OnRefreshComplete,
                   base::Unretained(this)));
  } else {
    OnRefreshComplete();
  }
}

void CloudPolicyManager::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_.get(), store);
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_.get(), store);
  // No action required, the old policy is still valid.
}

void CloudPolicyManager::InitializeService(
    scoped_ptr<CloudPolicyClient> client) {
  CHECK(!client_.get());
  CHECK(client.get());
  client_ = client.Pass();
  service_.reset(new CloudPolicyService(client_.get(), store_.get()));
}

void CloudPolicyManager::ShutdownService() {
  refresh_scheduler_.reset();
  service_.reset();
  client_.reset();
}

void CloudPolicyManager::StartRefreshScheduler(
    PrefService* local_state,
    const std::string& refresh_rate_pref) {
  if (!refresh_scheduler_.get()) {
    refresh_scheduler_.reset(
        new CloudPolicyRefreshScheduler(
            client_.get(), store_.get(), local_state, refresh_rate_pref,
            MessageLoop::current()->message_loop_proxy()));
  }
}

void CloudPolicyManager::CheckAndPublishPolicy() {
  if (IsInitializationComplete() && !waiting_for_policy_refresh_) {
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(
        store_->policy_map());
    UpdatePolicy(bundle.Pass());
  }
}

void CloudPolicyManager::OnRefreshComplete() {
  waiting_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
