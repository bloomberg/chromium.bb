// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/policy_info.h"

#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
constexpr char kPolicyName1[] = "policy_a";
constexpr char kPolicyName2[] = "policy_b";
}  // namespace

using ::testing::_;
using ::testing::Eq;

class PolicyInfoTest : public ::testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.SetPolicyService(GetPolicyService());
    profile_ = builder.Build();
  }

  std::unique_ptr<policy::MockPolicyService> GetPolicyService() {
    auto policy_service = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service.get(),
            GetPolicies(Eq(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                   std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
    policy_service_ = policy_service.get();
    return policy_service;
  }

  TestingProfile* profile() { return profile_.get(); }
  policy::PolicyMap* policy_map() { return &policy_map_; }
  policy::MockPolicyService* policy_service() { return policy_service_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService* policy_service_;
};

// Verify two Chrome policies are appended to the Profile report properly.
TEST_F(PolicyInfoTest, ChromePolicy) {
  policy_map()->Set(kPolicyName1, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(std::vector<base::Value>()),
                    nullptr);
  policy_map()->Set(kPolicyName2, policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    std::make_unique<base::Value>(true), nullptr);
  em::ChromeUserProfileInfo profile_info;

  EXPECT_CALL(*policy_service(), GetPolicies(_));

  AppendChromePolicyInfoIntoProfileReport(
      policy::GetAllPolicyValuesAsDictionary(
          profile(), /* with_user_policies */ true, /* convert_values */ false,
          /* with_device_data*/ false,
          /* is_pretty_print */ false, /* convert_types */ false),
      &profile_info);
  EXPECT_EQ(2, profile_info.chrome_policies_size());

  auto policy1 = profile_info.chrome_policies(0);
  EXPECT_EQ(kPolicyName1, policy1.name());
  EXPECT_EQ("[]", policy1.value());
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY, policy1.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, policy1.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, policy1.source());
  EXPECT_NE("", policy1.error());

  auto policy2 = profile_info.chrome_policies(1);
  EXPECT_EQ(kPolicyName2, policy2.name());
  EXPECT_EQ("true", policy2.value());
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_RECOMMENDED, policy2.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, policy2.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_MERGED, policy2.source());
  EXPECT_NE("", policy2.error());
}

}  // namespace enterprise_reporting
