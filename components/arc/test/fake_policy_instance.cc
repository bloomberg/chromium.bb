// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "components/arc/test/fake_policy_instance.h"

namespace arc {

FakePolicyInstance::FakePolicyInstance() = default;

FakePolicyInstance::~FakePolicyInstance() = default;

void FakePolicyInstance::InitDeprecated(mojom::PolicyHostPtr host_ptr) {
  Init(std::move(host_ptr), base::DoNothing());
}

void FakePolicyInstance::Init(mojom::PolicyHostPtr host_ptr,
                              InitCallback callback) {
  host_ptr_ = std::move(host_ptr);
  std::move(callback).Run();
}

void FakePolicyInstance::OnPolicyUpdated() {}

void FakePolicyInstance::CallGetPolicies(
    mojom::PolicyHost::GetPoliciesCallback callback) {
  host_ptr_->GetPolicies(std::move(callback));
  base::RunLoop().RunUntilIdle();
}

}  // namespace arc
