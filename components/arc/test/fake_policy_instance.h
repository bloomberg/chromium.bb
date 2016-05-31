// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_

#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/policy.mojom.h"
#include "components/arc/test/fake_arc_bridge_instance.h"

namespace arc {

class FakePolicyInstance : public mojom::PolicyInstance,
                           public arc::ArcBridgeService::Observer {
 public:
  // bridge_service should not be destroyed before the destructor is called.
  FakePolicyInstance(mojo::InterfaceRequest<mojom::PolicyInstance> request,
                     ArcBridgeService* bridge_service);
  ~FakePolicyInstance() override;

  // mojom::PolicyInstance
  void Init(mojom::PolicyHostPtr host_ptr) override;
  void OnPolicyUpdated() override;

  // arc::ArcBridgeService::Observer
  void OnPolicyInstanceReady() override;

  void WaitForOnPolicyInstanceReady();
  void CallGetPolicies(const mojom::PolicyHost::GetPoliciesCallback& callback);

 private:
  mojo::Binding<mojom::PolicyInstance> binding_;
  ArcBridgeService* const bridge_service_;
  mojom::PolicyHostPtr host_ptr_;

  bool ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakePolicyInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
