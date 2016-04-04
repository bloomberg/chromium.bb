// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "mojo/public/cpp/bindings/string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using testing::ReturnRef;

class ArcPolicyBridgeTest : public testing::Test {
 public:
  ArcPolicyBridgeTest() {}

  void SetUp() override {
    bridge_service_.reset(new FakeArcBridgeService());
    policy_bridge_.reset(
        new ArcPolicyBridge(bridge_service_.get(), &policy_service_));

    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));
  }

 protected:
  class PolicyStringRunnable
      : public ArcPolicyBridge::GetPoliciesCallback::Runnable {
   public:
    explicit PolicyStringRunnable(mojo::String expected)
        : expected_(expected) {}
    void Run(const mojo::String& policies) const override {
      EXPECT_EQ(expected_, policies);
    }

   private:
    mojo::String expected_;
  };

  class PolicyStringCallback : public ArcPolicyBridge::GetPoliciesCallback {
   public:
    explicit PolicyStringCallback(mojo::String expected)
        : ArcPolicyBridge::GetPoliciesCallback(
              new PolicyStringRunnable(expected)) {}
  };

  ArcPolicyBridge* policy_bridge() { return policy_bridge_.get(); }
  policy::PolicyMap& policy_map() { return policy_map_; }

 private:
  std::unique_ptr<arc::FakeArcBridgeService> bridge_service_;
  std::unique_ptr<arc::ArcPolicyBridge> policy_bridge_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridgeTest);
};

TEST_F(ArcPolicyBridgeTest, GetPoliciesTest) {
  PolicyStringCallback empty_callback("{}");
  policy_bridge()->GetPolicies(empty_callback);
  policy_map().Set("HomepageLocation", policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   new base::StringValue("http://chromium.org"), nullptr);
  policy_bridge()->GetPolicies(empty_callback);
  policy_map().Set(
      "ArcApplicationPolicy", policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      new base::StringValue("{\"application\": \"com.android.chrome\"}"),
      nullptr);
  PolicyStringCallback chrome_callback(
      "{\"ArcApplicationPolicy\":"
      "\"{\\\"application\\\": \\\"com.android.chrome\\\"}\"}");
  policy_bridge()->GetPolicies(chrome_callback);
}

}  // namespace arc
