// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "components/arc/test/fake_policy_instance.h"

namespace arc {

FakePolicyInstance::FakePolicyInstance() = default;

FakePolicyInstance::~FakePolicyInstance() = default;

void FakePolicyInstance::Init(mojom::PolicyHostPtr host_ptr) {
  host_ptr_ = std::move(host_ptr);
}

void FakePolicyInstance::OnPolicyUpdated() {}

void FakePolicyInstance::CallGetPolicies(
    const mojom::PolicyHost::GetPoliciesCallback& callback) {
  host_ptr_->GetPolicies(callback);
  base::RunLoop().RunUntilIdle();
}

}  // namespace arc
