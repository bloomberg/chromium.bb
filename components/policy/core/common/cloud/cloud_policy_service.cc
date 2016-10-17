// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_service.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

CloudPolicyService::CloudPolicyService(const std::string& policy_type,
                                       const std::string& settings_entity_id,
                                       CloudPolicyClient* client,
                                       CloudPolicyStore* store)
    : policy_type_(policy_type),
      settings_entity_id_(settings_entity_id),
      client_(client),
      store_(store),
      refresh_state_(REFRESH_NONE),
      unregister_state_(UNREGISTER_NONE),
      initialization_complete_(false) {
  client_->AddPolicyTypeToFetch(policy_type_, settings_entity_id_);
  client_->AddObserver(this);
  store_->AddObserver(this);

  // Make sure we initialize |client_| from the policy data that might be
  // already present in |store_|.
  OnStoreLoaded(store_);
}

CloudPolicyService::~CloudPolicyService() {
  client_->RemovePolicyTypeToFetch(policy_type_, settings_entity_id_);
  client_->RemoveObserver(this);
  store_->RemoveObserver(this);
}

std::string CloudPolicyService::ManagedBy() const {
  const em::PolicyData* policy = store_->policy();
  if (policy) {
    std::string username = policy->username();
    std::size_t pos = username.find('@');
    if (pos != std::string::npos)
      return username.substr(pos + 1);
  }
  return std::string();
}

void CloudPolicyService::RefreshPolicy(const RefreshPolicyCallback& callback) {
  // If the client is not registered or is unregistering, bail out.
  if (!client_->is_registered() || unregister_state_ != UNREGISTER_NONE) {
    callback.Run(false);
    return;
  }

  // Else, trigger a refresh.
  refresh_callbacks_.push_back(callback);
  refresh_state_ = REFRESH_POLICY_FETCH;
  client_->FetchPolicy();
}

void CloudPolicyService::Unregister(const UnregisterCallback& callback) {
  // Abort all pending refresh requests.
  if (refresh_state_ != REFRESH_NONE)
    RefreshCompleted(false);

  // Abort previous unregister request if any.
  if (unregister_state_  != UNREGISTER_NONE)
    UnregisterCompleted(false);

  unregister_callback_ = callback;
  unregister_state_ = UNREGISTER_PENDING;
  client_->Unregister();
}

void CloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  if (client_->status() != DM_STATUS_SUCCESS) {
    RefreshCompleted(false);
    return;
  }

  const em::PolicyFetchResponse* policy =
      client_->GetPolicyFor(policy_type_, settings_entity_id_);
  if (policy) {
    if (refresh_state_ != REFRESH_NONE)
      refresh_state_ = REFRESH_POLICY_STORE;
    store_->Store(*policy, client->fetched_invalidation_version());
  } else {
    RefreshCompleted(false);
  }
}

void CloudPolicyService::OnRegistrationStateChanged(CloudPolicyClient* client) {
  if (unregister_state_ == UNREGISTER_PENDING)
    UnregisterCompleted(true);
}

void CloudPolicyService::OnClientError(CloudPolicyClient* client) {
  if (refresh_state_ == REFRESH_POLICY_FETCH)
    RefreshCompleted(false);
  if (unregister_state_ == UNREGISTER_PENDING)
    UnregisterCompleted(false);
}

void CloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  // Update the client with state from the store.
  const em::PolicyData* policy(store_->policy());

  // Timestamp.
  base::Time policy_timestamp;
  if (policy && policy->has_timestamp()) {
    policy_timestamp =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(policy->timestamp());
  }
  client_->set_last_policy_timestamp(policy_timestamp);

  // Public key version.
  if (policy && policy->has_public_key_version())
    client_->set_public_key_version(policy->public_key_version());
  else
    client_->clear_public_key_version();

  // Whether to submit the machine ID.
  bool submit_machine_id = false;
  if (policy && policy->has_valid_serial_number_missing())
    submit_machine_id = policy->valid_serial_number_missing();
  client_->set_submit_machine_id(submit_machine_id);

  // Finally, set up registration if necessary.
  if (policy && policy->has_request_token() && policy->has_device_id() &&
      !client_->is_registered()) {
    DVLOG(1) << "Setting up registration with request token: "
             << policy->request_token();
    client_->SetupRegistration(policy->request_token(),
                               policy->device_id());
  }

  if (refresh_state_ == REFRESH_POLICY_STORE)
    RefreshCompleted(true);

  CheckInitializationCompleted();
}

void CloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  if (refresh_state_ == REFRESH_POLICY_STORE)
    RefreshCompleted(false);
  CheckInitializationCompleted();
}

void CloudPolicyService::CheckInitializationCompleted() {
  if (!IsInitializationComplete() && store_->is_initialized()) {
    initialization_complete_ = true;
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(this);
  }
}

void CloudPolicyService::RefreshCompleted(bool success) {
  // Clear state and |refresh_callbacks_| before actually invoking them, s.t.
  // triggering new policy fetches behaves as expected.
  std::vector<RefreshPolicyCallback> callbacks;
  callbacks.swap(refresh_callbacks_);
  refresh_state_ = REFRESH_NONE;

  for (std::vector<RefreshPolicyCallback>::iterator callback(callbacks.begin());
       callback != callbacks.end();
       ++callback) {
    callback->Run(success);
  }
}

void CloudPolicyService::UnregisterCompleted(bool success) {
  if (!success)
    LOG(ERROR) << "Unregister request failed.";

  unregister_state_ = UNREGISTER_NONE;
  unregister_callback_.Run(success);
  unregister_callback_ = UnregisterCallback();  // Reset.
}

void CloudPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace policy
