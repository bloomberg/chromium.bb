// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_store.h"

namespace policy {

CloudPolicyStore::Observer::~Observer() {}

CloudPolicyStore::CloudPolicyStore()
    : status_(STATUS_OK),
      validation_status_(CloudPolicyValidatorBase::VALIDATION_OK),
      is_initialized_(false) {}

CloudPolicyStore::~CloudPolicyStore() {}

void CloudPolicyStore::AddObserver(CloudPolicyStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyStore::RemoveObserver(CloudPolicyStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyStore::NotifyStoreLoaded() {
  is_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStoreLoaded(this));
}

void CloudPolicyStore::NotifyStoreError() {
  is_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStoreError(this));
}

}  // namespace
