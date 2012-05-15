// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_stub.h"

#include "base/message_loop.h"

namespace policy {

PolicyServiceStub::PolicyServiceStub() {}

PolicyServiceStub::~PolicyServiceStub() {}

void PolicyServiceStub::AddObserver(PolicyDomain domain,
                                    const std::string& component_id,
                                    Observer* observer) {}

void PolicyServiceStub::RemoveObserver(PolicyDomain domain,
                                       const std::string& component_id,
                                       Observer* observer) {}

const PolicyMap& PolicyServiceStub::GetPolicies(
    PolicyDomain domain,
    const std::string& component_id) const {
  return kEmpty_;
};

bool PolicyServiceStub::IsInitializationComplete() const {
  return true;
}

void PolicyServiceStub::RefreshPolicies(const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
}

}  // namespace policy
