// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_MOCK_POLICY_SERVICE_H_

#include "chrome/browser/policy/policy_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockPolicyService : public PolicyService {
 public:
  MockPolicyService();
  ~MockPolicyService();

  MOCK_METHOD2(AddObserver, void(PolicyDomain, Observer*));
  MOCK_METHOD2(RemoveObserver, void(PolicyDomain, Observer*));

  // vs2010 doesn't compile this:
  // MOCK_METHOD1(RegisterPolicyNamespace, void(const PolicyNamespace&));
  // MOCK_METHOD1(UnregisterPolicyNamespace, void(const PolicyNamespace&));

  // Tests use these 2 mock methods instead:
  MOCK_METHOD2(RegisterPolicyNamespace, void(PolicyDomain,
                                             const std::string&));
  MOCK_METHOD2(UnregisterPolicyNamespace, void(PolicyDomain,
                                               const std::string&));

  // And the overridden calls just forward to the new mock methods:
  virtual void RegisterPolicyNamespace(const PolicyNamespace& ns) OVERRIDE {
    RegisterPolicyNamespace(ns.domain, ns.component_id);
  }

  virtual void UnregisterPolicyNamespace(const PolicyNamespace& ns) OVERRIDE {
    UnregisterPolicyNamespace(ns.domain, ns.component_id);
  }

  MOCK_CONST_METHOD1(GetPolicies, const PolicyMap&(const PolicyNamespace&));
  MOCK_CONST_METHOD1(IsInitializationComplete, bool(PolicyDomain domain));
  MOCK_METHOD1(RefreshPolicies, void(const base::Closure&));
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_POLICY_SERVICE_H_
