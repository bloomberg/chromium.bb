// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::_;

namespace policy {

namespace {

class MockPolicyServiceObserver : public PolicyService::Observer {
 public:
  virtual ~MockPolicyServiceObserver() {}
  MOCK_METHOD4(OnPolicyUpdated, void(PolicyDomain,
                                     const std::string&,
                                     const PolicyMap& previous,
                                     const PolicyMap& current));
};

// Helper to compare the arguments to an EXPECT_CALL of OnPolicyUpdated() with
// their expected values.
MATCHER_P(PolicyEquals, expected, "") {
  return arg.Equals(*expected);
}

// Helper to compare the arguments to an EXPECT_CALL of OnPolicyValueUpdated()
// with their expected values.
MATCHER_P(ValueEquals, expected, "") {
  return base::Value::Equals(arg, expected);
}

}  // namespace

class PolicyServiceTest : public testing::Test {
 public:
  PolicyServiceTest() {}

  void SetUp() OVERRIDE {
    provider0_.AddMandatoryPolicy("pre", base::Value::CreateIntegerValue(13));
    provider0_.SetInitializationComplete(true);
    provider1_.SetInitializationComplete(true);
    provider2_.SetInitializationComplete(true);
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider0_);
    providers.push_back(&provider1_);
    providers.push_back(&provider2_);
    policy_service_.reset(new PolicyServiceImpl(providers));
  }

  MOCK_METHOD2(OnPolicyValueUpdated, void(const base::Value*,
                                          const base::Value*));

  // Returns true if the policies for |domain|, |component_id| match |expected|.
  bool VerifyPolicies(PolicyDomain domain,
                      const std::string& component_id,
                      const PolicyMap& expected) {
    const PolicyMap* policies =
        policy_service_->GetPolicies(domain, component_id);
    return policies && policies->Equals(expected);
  }

 protected:
  MockConfigurationPolicyProvider provider0_;
  MockConfigurationPolicyProvider provider1_;
  MockConfigurationPolicyProvider provider2_;
  scoped_ptr<PolicyServiceImpl> policy_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyServiceTest);
};

TEST_F(PolicyServiceTest, LoadsPoliciesBeforeProvidersRefresh) {
  PolicyMap expected;
  expected.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(13));
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));
}

TEST_F(PolicyServiceTest, NotifyObservers) {
  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, "", &observer);

  PolicyMap expectedPrevious;
  expectedPrevious.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       base::Value::CreateIntegerValue(13));

  PolicyMap expectedCurrent;
  expectedCurrent.CopyFrom(expectedPrevious);
  expectedCurrent.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateIntegerValue(123));
  provider0_.AddMandatoryPolicy("aaa", base::Value::CreateIntegerValue(123));
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  // No changes.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expectedCurrent));

  // New policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateIntegerValue(456));
  provider0_.AddMandatoryPolicy("bbb", base::Value::CreateIntegerValue(456));
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  // Removed policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Erase("bbb");
  provider0_.RemovePolicy("bbb");
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  // Changed policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateIntegerValue(789));
  provider0_.AddMandatoryPolicy("aaa", base::Value::CreateIntegerValue(789));
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  // No changes again.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expectedCurrent));

  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, "", &observer);
}

TEST_F(PolicyServiceTest, Priorities) {
  PolicyMap expected;
  expected.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(13));
  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(0));
  provider0_.AddMandatoryPolicy("aaa", base::Value::CreateIntegerValue(0));
  provider1_.AddMandatoryPolicy("aaa", base::Value::CreateIntegerValue(1));
  provider2_.AddMandatoryPolicy("aaa", base::Value::CreateIntegerValue(2));
  provider0_.RefreshPolicies();
  provider1_.RefreshPolicies();
  provider2_.RefreshPolicies();
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  provider0_.RemovePolicy("aaa");
  provider0_.RefreshPolicies();
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(2));
  provider1_.AddRecommendedPolicy("aaa", base::Value::CreateIntegerValue(1));
  provider1_.RefreshPolicies();
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));
}

TEST_F(PolicyServiceTest, PolicyChangeRegistrar) {
  scoped_ptr<PolicyChangeRegistrar> registrar(
      new PolicyChangeRegistrar(
          policy_service_.get(), POLICY_DOMAIN_CHROME, ""));

  // Starting to observe existing policies doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar->Observe("pre", base::Bind(
      &PolicyServiceTest::OnPolicyValueUpdated,
      base::Unretained(this)));
  registrar->Observe("aaa", base::Bind(
      &PolicyServiceTest::OnPolicyValueUpdated,
      base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Changing it now triggers a notification.
  base::FundamentalValue kValue0(0);
  EXPECT_CALL(*this, OnPolicyValueUpdated(NULL, ValueEquals(&kValue0)));
  provider0_.AddMandatoryPolicy("aaa", kValue0.DeepCopy());
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // Changing other values doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  provider0_.AddMandatoryPolicy("bbb", kValue0.DeepCopy());
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // Modifying the value triggers a notification.
  base::FundamentalValue kValue1(1);
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue0),
                                          ValueEquals(&kValue1)));
  provider0_.AddMandatoryPolicy("aaa", kValue1.DeepCopy());
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // Removing the value triggers a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue1), NULL));
  provider0_.RemovePolicy("aaa");
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // No more notifications after destroying the registrar.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar.reset();
  provider0_.AddMandatoryPolicy("aaa", kValue1.DeepCopy());
  provider0_.AddMandatoryPolicy("pre", kValue1.DeepCopy());
  provider0_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);
}

}  // namespace policy
