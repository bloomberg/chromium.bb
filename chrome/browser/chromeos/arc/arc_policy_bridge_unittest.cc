// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "mojo/public/cpp/bindings/string.h"
#include "policy/policy_constants.h"
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
    policy_bridge_->OverrideIsManagedForTesting(true);

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
    void Run(const mojo::String& policies) override {
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

TEST_F(ArcPolicyBridgeTest, UnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  policy_bridge()->GetPolicies(PolicyStringCallback(nullptr));
}

TEST_F(ArcPolicyBridgeTest, EmptyPolicyTest) {
  // No policy is set, result should be empty.
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, ArcPolicyTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      new base::StringValue(
          "{\"applications\":"
              "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                "\"installType\":\"REQUIRED\","
                "\"lockTaskAllowed\":false,"
                "\"permissionGrants\":[]"
              "}],"
          "\"defaultPermissionPolicy\":\"GRANT\""
          "}"),
      nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
          "[{\"installType\":\"REQUIRED\","
            "\"lockTaskAllowed\":false,"
            "\"packageName\":\"com.google.android.apps.youtube.kids\","
            "\"permissionGrants\":[]"
          "}],"
      "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

TEST_F(ArcPolicyBridgeTest, HompageLocationTest) {
  // This policy will not be passed on, result should be empty.
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   new base::StringValue("http://chromium.org"),
                   nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, DisableScreenshotsTest) {
  policy_map().Set(policy::key::kDisableScreenshots,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   new base::FundamentalValue(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"screenCaptureDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, VideoCaptureAllowedTest) {
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   new base::FundamentalValue(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"cameraDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, AudioCaptureAllowedTest) {
  policy_map().Set(policy::key::kAudioCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   new base::FundamentalValue(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"unmuteMicrophoneDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, DefaultGeolocationSettingTest) {
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, new base::FundamentalValue(1),
                   nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, new base::FundamentalValue(2),
                   nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":true}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, new base::FundamentalValue(3),
                   nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
}

TEST_F(ArcPolicyBridgeTest, URLBlacklistTest) {
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("www.blacklist1.com"));
  blacklist.Append(new base::StringValue("www.blacklist2.com"));
  policy_map().Set(policy::key::kURLBlacklist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   blacklist.DeepCopy(), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"globalAppRestrictions\":"
                           "{\"com.android.browser:URLBlacklist\":"
                           "[\"www.blacklist1.com\","
                           "\"www.blacklist2.com\""
                           "]}}"));
}

TEST_F(ArcPolicyBridgeTest, URLWhitelistTest) {
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("www.whitelist1.com"));
  whitelist.Append(new base::StringValue("www.whitelist2.com"));
  policy_map().Set(policy::key::kURLWhitelist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   whitelist.DeepCopy(), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"globalAppRestrictions\":"
                           "{\"com.android.browser:URLWhitelist\":"
                           "[\"www.whitelist1.com\","
                           "\"www.whitelist2.com\""
                           "]}}"));
}

TEST_F(ArcPolicyBridgeTest, MultiplePoliciesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      new base::StringValue("{\"applications\":"
              "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                "\"installType\":\"REQUIRED\","
                "\"lockTaskAllowed\":false,"
                "\"permissionGrants\":[]"
              "}],"
          "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   new base::StringValue("http://chromium.org"), nullptr);
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   new base::FundamentalValue(false), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
          "[{\"installType\":\"REQUIRED\","
            "\"lockTaskAllowed\":false,"
            "\"packageName\":\"com.google.android.apps.youtube.kids\","
            "\"permissionGrants\":[]"
          "}],"
        "\"cameraDisabled\":true,"
        "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

}  // namespace arc
