// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

namespace policy {

namespace {

void IgnoreCallback() {
}

}  // namespace

class AsynchronousPolicyLoaderTest : public AsynchronousPolicyTestBase {
 public:
  AsynchronousPolicyLoaderTest() {}
  virtual ~AsynchronousPolicyLoaderTest() {}

  virtual void SetUp() {
    AsynchronousPolicyTestBase::SetUp();
    ignore_callback_ = base::Bind(&IgnoreCallback);
  }

 protected:
  base::Closure ignore_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyLoaderTest);
};

ACTION(CreateTestPolicyMap) {
  return new PolicyMap();
}

ACTION_P(CreateSequencedTestPolicyMap, number) {
  PolicyMap* test_policy_map = new PolicyMap();
  test_policy_map->Set("id", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       Value::CreateIntegerValue(++(*number)));
  return test_policy_map;
}

ACTION(RescheduleImmediatePolicyReload) {
  *arg1 = base::TimeDelta();
  return false;
}

TEST_F(AsynchronousPolicyLoaderTest, InitialLoad) {
  PolicyMap template_policy;
  template_policy.Set("test", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      Value::CreateIntegerValue(123));
  PolicyMap* result = new PolicyMap();
  result->CopyFrom(template_policy);
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(Return(result));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  const PolicyMap& loaded_policy(loader->policy());
  EXPECT_TRUE(loaded_policy.Equals(template_policy));
}

// Verify that the fallback policy requests are made.
TEST_F(AsynchronousPolicyLoaderTest, InitialLoadWithFallback) {
  int policy_number = 0;
  InSequence s;
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestPolicyMap(&policy_number));
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestPolicyMap(&policy_number));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  loop_.RunAllPending();
  loader->Reload(true);
  loop_.RunAllPending();

  const PolicyMap& loaded_policy(loader->policy());
  base::FundamentalValue expected(policy_number);
  EXPECT_TRUE(Value::Equals(&expected, loaded_policy.GetValue("id")));
  EXPECT_EQ(1U, loaded_policy.size());
}

// Ensure that calling stop on the loader stops subsequent reloads from
// happening.
TEST_F(AsynchronousPolicyLoaderTest, Stop) {
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  ON_CALL(*delegate, Load()).WillByDefault(CreateTestPolicyMap());
  EXPECT_CALL(*delegate, Load()).Times(1);
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  loop_.RunAllPending();
  loader->Stop();
  loop_.RunAllPending();
  loader->Reload(true);
  loop_.RunAllPending();
}

// Verifies that the provider is notified upon policy reload.
TEST_F(AsynchronousPolicyLoaderTest, ProviderNotificationOnPolicyChange) {
  InSequence s;
  MockConfigurationPolicyObserver observer;
  int policy_number_1 = 0;
  int policy_number_2 = 0;

  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestPolicyMap(&policy_number_1));

  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  AsynchronousPolicyProvider provider(NULL, loader);
  // |registrar| must be declared last so that it is destroyed first.
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&provider, &observer);
  Mock::VerifyAndClearExpectations(delegate);

  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestPolicyMap(&policy_number_2));
  EXPECT_CALL(observer, OnUpdatePolicy(_)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(delegate);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestPolicyMap(&policy_number_1));
  EXPECT_CALL(observer, OnUpdatePolicy(_)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(delegate);
  Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace policy
