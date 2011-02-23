// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_identity_strategy.h"

namespace policy {

CloudPolicyIdentityStrategy::CloudPolicyIdentityStrategy() {}

CloudPolicyIdentityStrategy::~CloudPolicyIdentityStrategy() {}

void CloudPolicyIdentityStrategy::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void CloudPolicyIdentityStrategy::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void CloudPolicyIdentityStrategy::NotifyDeviceTokenChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceTokenChanged());
}

void CloudPolicyIdentityStrategy::NotifyAuthChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnCredentialsChanged());
}

}  // namespace policy
