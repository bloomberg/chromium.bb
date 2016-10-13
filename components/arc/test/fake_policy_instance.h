// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_

#include "components/arc/common/policy.mojom.h"

namespace arc {

class FakePolicyInstance : public mojom::PolicyInstance {
 public:
  FakePolicyInstance();
  ~FakePolicyInstance() override;

  // mojom::PolicyInstance
  void Init(mojom::PolicyHostPtr host_ptr) override;
  void OnPolicyUpdated() override;

  void CallGetPolicies(const mojom::PolicyHost::GetPoliciesCallback& callback);

 private:
  mojom::PolicyHostPtr host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FakePolicyInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
