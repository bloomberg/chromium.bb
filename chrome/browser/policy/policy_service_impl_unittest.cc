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

const char kExtension[] = "extension-id";
const char kSameLevelPolicy[] = "policy-same-level-and-scope";
const char kDiffLevelPolicy[] = "chrome-diff-level-and-scope";

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

// Helper that fills |bundle| with test policies.
void AddTestPolicies(PolicyBundle* bundle,
                     const char* value,
                     PolicyLevel level,
                     PolicyScope scope) {
  PolicyMap* policy_map = &bundle->Get(POLICY_DOMAIN_CHROME, "");
  policy_map->Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  base::Value::CreateStringValue(value));
  policy_map->Set(kDiffLevelPolicy, level, scope,
                  base::Value::CreateStringValue(value));
  policy_map = &bundle->Get(POLICY_DOMAIN_EXTENSIONS, kExtension);
  policy_map->Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, base::Value::CreateStringValue(value));
  policy_map->Set(kDiffLevelPolicy, level, scope,
                  base::Value::CreateStringValue(value));
}

}  // namespace

class PolicyServiceTest : public testing::Test {
 public:
  PolicyServiceTest() {}

  void SetUp() OVERRIDE {
    EXPECT_CALL(provider0_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider1_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider2_, IsInitializationComplete())
        .WillRepeatedly(Return(true));

    policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(13));
    provider0_.UpdateChromePolicy(policy0_);

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
    return policy_service_->GetPolicies(domain, component_id).Equals(expected);
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
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // No changes.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.UpdateChromePolicy(policy0_);
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
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // Removed policy.
  expectedPrevious.CopyFrom(expectedCurrent);
  expectedCurrent.Erase("bbb");
  policy0_.Erase("bbb");
  EXPECT_CALL(observer, OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.UpdateChromePolicy(policy0_);
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
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // No changes again.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expectedCurrent));

  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, "", &observer);
}

TEST_F(PolicyServiceTest, NotifyObserversInMultipleNamespaces) {
  const std::string kExtension0("extension-0");
  const std::string kExtension1("extension-1");
  const std::string kExtension2("extension-2");
  MockPolicyServiceObserver chrome_observer;
  MockPolicyServiceObserver extension0_observer;
  MockPolicyServiceObserver extension1_observer;
  MockPolicyServiceObserver extension2_observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, "", &chrome_observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, kExtension0,
                               &extension0_observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, kExtension1,
                               &extension1_observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, kExtension2,
                               &extension2_observer);

  PolicyMap previous_policy_map;
  previous_policy_map.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                          base::Value::CreateIntegerValue(13));
  PolicyMap policy_map;
  policy_map.CopyFrom(previous_policy_map);
  policy_map.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("value"));

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  // The initial setup includes a policy for chrome that is now changing.
  bundle->Get(POLICY_DOMAIN_CHROME, "").CopyFrom(policy_map);
  bundle->Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).CopyFrom(policy_map);
  bundle->Get(POLICY_DOMAIN_EXTENSIONS, kExtension1).CopyFrom(policy_map);

  const PolicyMap kEmptyPolicyMap;
  EXPECT_CALL(chrome_observer,
              OnPolicyUpdated(POLICY_DOMAIN_CHROME, "",
                              PolicyEquals(&previous_policy_map),
                              PolicyEquals(&policy_map)));
  EXPECT_CALL(extension0_observer,
              OnPolicyUpdated(POLICY_DOMAIN_EXTENSIONS, kExtension0,
                              PolicyEquals(&kEmptyPolicyMap),
                              PolicyEquals(&policy_map)));
  EXPECT_CALL(extension1_observer,
              OnPolicyUpdated(POLICY_DOMAIN_EXTENSIONS, kExtension1,
                              PolicyEquals(&kEmptyPolicyMap),
                              PolicyEquals(&policy_map)));
  EXPECT_CALL(extension2_observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  provider0_.UpdatePolicy(bundle.Pass());
  Mock::VerifyAndClearExpectations(&chrome_observer);
  Mock::VerifyAndClearExpectations(&extension0_observer);
  Mock::VerifyAndClearExpectations(&extension1_observer);
  Mock::VerifyAndClearExpectations(&extension2_observer);

  // Chrome policy stays the same, kExtension0 is gone, kExtension1 changes,
  // and kExtension2 is new.
  previous_policy_map.CopyFrom(policy_map);
  bundle.reset(new PolicyBundle());
  bundle->Get(POLICY_DOMAIN_CHROME, "").CopyFrom(policy_map);
  policy_map.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("another value"));
  bundle->Get(POLICY_DOMAIN_EXTENSIONS, kExtension1).CopyFrom(policy_map);
  bundle->Get(POLICY_DOMAIN_EXTENSIONS, kExtension2).CopyFrom(policy_map);

  EXPECT_CALL(chrome_observer, OnPolicyUpdated(_, _, _, _)).Times(0);
  EXPECT_CALL(extension0_observer,
              OnPolicyUpdated(POLICY_DOMAIN_EXTENSIONS, kExtension0,
                              PolicyEquals(&previous_policy_map),
                              PolicyEquals(&kEmptyPolicyMap)));
  EXPECT_CALL(extension1_observer,
              OnPolicyUpdated(POLICY_DOMAIN_EXTENSIONS, kExtension1,
                              PolicyEquals(&previous_policy_map),
                              PolicyEquals(&policy_map)));
  EXPECT_CALL(extension2_observer,
              OnPolicyUpdated(POLICY_DOMAIN_EXTENSIONS, kExtension2,
                              PolicyEquals(&kEmptyPolicyMap),
                              PolicyEquals(&policy_map)));
  provider0_.UpdatePolicy(bundle.Pass());
  Mock::VerifyAndClearExpectations(&chrome_observer);
  Mock::VerifyAndClearExpectations(&extension0_observer);
  Mock::VerifyAndClearExpectations(&extension1_observer);
  Mock::VerifyAndClearExpectations(&extension2_observer);

  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, "", &chrome_observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, kExtension0,
                                  &extension0_observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, kExtension1,
                                  &extension1_observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, kExtension2,
                                  &extension2_observer);
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
  provider0_.UpdateChromePolicy(policy0_);
  provider1_.UpdateChromePolicy(policy1_);
  provider2_.UpdateChromePolicy(policy2_);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  policy0_.Erase("aaa");
  provider0_.UpdateChromePolicy(policy0_);
  EXPECT_TRUE(VerifyPolicies(POLICY_DOMAIN_CHROME, "", expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(2));
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  provider1_.UpdateChromePolicy(policy1_);
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
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Changing other values doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue0.DeepCopy());
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Modifying the value triggers a notification.
  base::FundamentalValue kValue1(1);
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue0),
                                          ValueEquals(&kValue1)));
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Removing the value triggers a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue1), NULL));
  policy0_.Erase("aaa");
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // No more notifications after destroying the registrar.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar.reset();
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider0_.UpdateChromePolicy(policy0_);
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
  provider0_.UpdateChromePolicy(policy0_);
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  base::FundamentalValue kValue1(1);
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider1_.UpdateChromePolicy(policy1_);
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  // A provider can refresh more than once after a RefreshPolicies call, but
  // OnPolicyRefresh should be triggered only after all providers are
  // refreshed.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy1_.Set("bbb", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               kValue1.DeepCopy());
  provider1_.UpdateChromePolicy(policy1_);
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
  provider2_.UpdateChromePolicy(policy2_);
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  // Providers 0 and 1 must reload again.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(2);
  base::FundamentalValue kValue2(2);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               kValue2.DeepCopy());
  provider0_.UpdateChromePolicy(policy0_);
  provider1_.UpdateChromePolicy(policy1_);
  loop.RunAllPending();
  Mock::VerifyAndClearExpectations(this);

  const PolicyMap& policies = policy_service_->GetPolicies(
      POLICY_DOMAIN_CHROME, "");
  EXPECT_TRUE(base::Value::Equals(&kValue2, policies.GetValue("aaa")));
  EXPECT_TRUE(base::Value::Equals(&kValue0, policies.GetValue("bbb")));
}

TEST_F(PolicyServiceTest, NamespaceMerge) {
  scoped_ptr<PolicyBundle> bundle0(new PolicyBundle());
  scoped_ptr<PolicyBundle> bundle1(new PolicyBundle());
  scoped_ptr<PolicyBundle> bundle2(new PolicyBundle());

  AddTestPolicies(bundle0.get(), "bundle0",
                  POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);
  AddTestPolicies(bundle1.get(), "bundle1",
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  AddTestPolicies(bundle2.get(), "bundle2",
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);

  provider0_.UpdatePolicy(bundle0.Pass());
  provider1_.UpdatePolicy(bundle1.Pass());
  provider2_.UpdatePolicy(bundle2.Pass());

  PolicyMap expected;
  // For policies of the same level and scope, the first provider takes
  // precedence, on every namespace.
  expected.Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateStringValue("bundle0"));
  // For policies with different levels and scopes, the highest priority
  // level/scope combination takes precedence, on every namespace.
  expected.Set(kDiffLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               base::Value::CreateStringValue("bundle2"));
  EXPECT_TRUE(policy_service_->GetPolicies(POLICY_DOMAIN_CHROME, "")
      .Equals(expected));
  EXPECT_TRUE(policy_service_->GetPolicies(POLICY_DOMAIN_EXTENSIONS, kExtension)
      .Equals(expected));
}

}  // namespace policy
