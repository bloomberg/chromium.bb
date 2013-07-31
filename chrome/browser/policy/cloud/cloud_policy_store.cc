// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_store.h"

#include "base/hash.h"

namespace policy {

CloudPolicyStore::Observer::~Observer() {}

CloudPolicyStore::CloudPolicyStore()
    : status_(STATUS_OK),
      validation_status_(CloudPolicyValidatorBase::VALIDATION_OK),
      invalidation_version_(0),
      is_initialized_(false),
      policy_changed_(false),
      hash_value_(0) {}

CloudPolicyStore::~CloudPolicyStore() {}

void CloudPolicyStore::Store(
    const enterprise_management::PolicyFetchResponse& policy,
    int64 invalidation_version) {
  invalidation_version_ = invalidation_version;
  Store(policy);
}

void CloudPolicyStore::AddObserver(CloudPolicyStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyStore::RemoveObserver(CloudPolicyStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyStore::NotifyStoreLoaded() {
  // Determine if the policy changed by comparing the new policy's hash value
  // to the previous.
  uint32 new_hash_value = 0;
  if (policy_ && policy_->has_policy_value())
    new_hash_value = base::Hash(policy_->policy_value());
  policy_changed_ = new_hash_value != hash_value_;
  hash_value_ = new_hash_value;

  is_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStoreLoaded(this));
}

void CloudPolicyStore::NotifyStoreError() {
  is_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStoreError(this));
}

}  // namespace policy
