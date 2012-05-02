// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
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
    EXPECT_CALL(provider0_, ProvideInternal(_))
        .WillRepeatedly(CopyPolicyMap(&policy0_));
    EXPECT_CALL(provider1_, ProvideInternal(_))
        .WillRepeatedly(CopyPolicyMap(&policy1_));
    EXPECT_CALL(provider2_, ProvideInternal(_))
        .WillRepeatedly(CopyPolicyMap(&policy2_));

    EXPECT_CALL(provider0_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider1_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider2_, IsInitializationComplete())
        .WillRepeatedly(Return(true));

    policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(13));

    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider0_);
    providers.push_back(&provider1_);
    providers.push_back(&provider2_);
    policy_service_.reset(new PolicyServiceImpl(providers));
  }

  MOCK_METHOD2(OnPolicyValueUpdated, void(const base::Value*,
                                          const base::Value*));

  MOCK_METHOD0(OnPolicyRefresh, void());

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
  PolicyMap policy0_;
  PolicyMap policy1_;
  PolicyMap policy2_;
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
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(123));
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&observer);

  // No changes.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expectedCurrent));

  // New policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateIntegerValue(456));
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(456));
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&observer);

  // Removed policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Erase("bbb");
  policy0_.Erase("bbb");
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&observer);

  // Changed policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateIntegerValue(789));
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(789));

  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&observer);

  // No changes again.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.NotifyPolicyUpdated();
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
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(0));
  policy1_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  policy2_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(2));
  provider0_.NotifyPolicyUpdated();
  provider1_.NotifyPolicyUpdated();
  provider2_.NotifyPolicyUpdated();
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  policy0_.Erase("aaa");
  provider0_.NotifyPolicyUpdated();
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(2));
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  provider1_.NotifyPolicyUpdated();
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
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue0.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(this);

  // Changing other values doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue0.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(this);

  // Modifying the value triggers a notification.
  base::FundamentalValue kValue1(1);
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue0),
                                          ValueEquals(&kValue1)));
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(this);

  // Removing the value triggers a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue1), NULL));
  policy0_.Erase("aaa");
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(this);

  // No more notifications after destroying the registrar.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar.reset();
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(PolicyServiceTest, RefreshPolicies) {
  MessageLoop loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI, &loop);
  content::TestBrowserThread file_thread(content::BrowserThread::FILE, &loop);
  content::TestBrowserThread io_thread(content::BrowserThread::IO, &loop);

  EXPECT_CALL(provider0_, RefreshPolicies()).Times(AnyNumber());
  EXPECT_CALL(provider1_, RefreshPolicies()).Times(AnyNumber());
  EXPECT_CALL(provider2_, RefreshPolicies()).Times(AnyNumber());

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy_service_->RefreshPolicies(base::Bind(
      &PolicyServiceTest::OnPolicyRefresh,
      base::Unretained(this)));
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  base::FundamentalValue kValue0(0);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue0.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  base::FundamentalValue kValue1(1);
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider1_.NotifyPolicyUpdated();
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  // A provider can refresh more than once after a RefreshPolicies call, but
  // OnPolicyRefresh should be triggered only after all providers are
  // refreshed.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy1_.Set("bbb", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider1_.NotifyPolicyUpdated();
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  // If another RefreshPolicies() call happens while waiting for a previous
  // one to complete, then all providers must refresh again.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy_service_->RefreshPolicies(base::Bind(
      &PolicyServiceTest::OnPolicyRefresh,
      base::Unretained(this)));
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy2_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue0.DeepCopy());
  provider2_.NotifyPolicyUpdated();
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  // Providers 0 and 1 must reload again.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(2);
  base::FundamentalValue kValue2(2);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue2.DeepCopy());
  provider0_.NotifyPolicyUpdated();
  provider1_.NotifyPolicyUpdated();
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  const PolicyMap* policies = policy_service_->GetPolicies(
      POLICY_DOMAIN_CHROME, "");
  ASSERT_TRUE(policies);
  EXPECT_TRUE(base::Value::Equals(&kValue2, policies->GetValue("aaa")));
  EXPECT_TRUE(base::Value::Equals(&kValue0, policies->GetValue("bbb")));
}

}  // namespace policy
