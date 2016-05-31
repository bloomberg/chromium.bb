// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_policy_instance.h"
#include "base/run_loop.h"

namespace arc {

FakePolicyInstance::FakePolicyInstance(
    mojo::InterfaceRequest<mojom::PolicyInstance> request,
    ArcBridgeService* bridge_service)
    : binding_(this, std::move(request)), bridge_service_(bridge_service) {
  bridge_service_->AddObserver(this);
}

FakePolicyInstance::~FakePolicyInstance() {
  bridge_service_->RemoveObserver(this);
}

void FakePolicyInstance::OnPolicyUpdated() {}

void FakePolicyInstance::Init(mojom::PolicyHostPtr host_ptr) {
  host_ptr_ = std::move(host_ptr);
}

void FakePolicyInstance::OnPolicyInstanceReady() {
  ready_ = true;
}

void FakePolicyInstance::WaitForOnPolicyInstanceReady() {
  while (!ready_) {
    base::RunLoop().RunUntilIdle();
  }
}

void FakePolicyInstance::CallGetPolicies(
    const mojom::PolicyHost::GetPoliciesCallback& callback) {
  host_ptr_->GetPolicies(callback);
  base::RunLoop().RunUntilIdle();
}

}  // namespace arc
