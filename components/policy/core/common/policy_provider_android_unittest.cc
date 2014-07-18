// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/policy_provider_android.h"
#include "components/policy/core/common/policy_provider_android_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Helper to write a policy in |bundle| with less code.
void SetPolicy(PolicyBundle* bundle,
               const std::string& name,
               const std::string& value) {
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(name,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           new base::StringValue(value),
           NULL);
}

class MockPolicyProviderAndroidDelegate : public PolicyProviderAndroidDelegate {
 public:
  MockPolicyProviderAndroidDelegate() {}
  virtual ~MockPolicyProviderAndroidDelegate() {}

  MOCK_METHOD0(RefreshPolicies, void());
  MOCK_METHOD0(PolicyProviderShutdown, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyProviderAndroidDelegate);
};

// Test fixture that makes sure that we always call Shutdown() before destroying
// the policy provider. Allocate this just like a PolicyProviderAndroid and use
// Get() to get the policy provider.
class PolicyProviderAndroidTestFixture {
 public:
  PolicyProviderAndroidTestFixture() {}
  ~PolicyProviderAndroidTestFixture() {
    provider_.Shutdown();
  }

  PolicyProviderAndroid* Get() {
    return &provider_;
  }

 private:
  PolicyProviderAndroid provider_;
  DISALLOW_COPY_AND_ASSIGN(PolicyProviderAndroidTestFixture);
};

}  // namespace

class PolicyProviderAndroidTest : public ::testing::Test {
 protected:
  PolicyProviderAndroidTest();
  virtual ~PolicyProviderAndroidTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyProviderAndroidTest);
};

PolicyProviderAndroidTest::PolicyProviderAndroidTest() {}
PolicyProviderAndroidTest::~PolicyProviderAndroidTest() {}

void PolicyProviderAndroidTest::SetUp() {}

void PolicyProviderAndroidTest::TearDown() {
  PolicyProviderAndroid::SetShouldWaitForPolicy(false);
}

TEST_F(PolicyProviderAndroidTest, InitializationCompleted) {
  PolicyProviderAndroidTestFixture provider;
  EXPECT_TRUE(provider.Get()->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider.Get()->policies().Equals(kEmptyBundle));
}

TEST_F(PolicyProviderAndroidTest, WaitForInitialization) {
  PolicyProviderAndroid::SetShouldWaitForPolicy(true);
  PolicyProviderAndroidTestFixture provider;
  EXPECT_FALSE(provider.Get()->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  SetPolicy(policy_bundle.get(), "key", "value");
  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.CopyFrom(*policy_bundle);
  provider.Get()->SetPolicies(policy_bundle.Pass());
  EXPECT_TRUE(provider.Get()->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(provider.Get()->policies().Equals(expected_policy_bundle));
}

TEST_F(PolicyProviderAndroidTest, RefreshPolicies) {
  MockPolicyProviderAndroidDelegate delegate;
  PolicyProviderAndroidTestFixture provider;

  provider.Get()->SetDelegate(&delegate);

  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  SetPolicy(policy_bundle.get(), "key", "old_value");
  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.CopyFrom(*policy_bundle);
  provider.Get()->SetPolicies(policy_bundle.Pass());
  EXPECT_TRUE(provider.Get()->policies().Equals(expected_policy_bundle));

  EXPECT_CALL(delegate, RefreshPolicies()).Times(1);
  provider.Get()->RefreshPolicies();

  policy_bundle.reset(new PolicyBundle);
  SetPolicy(policy_bundle.get(), "key", "new_value");
  expected_policy_bundle.CopyFrom(*policy_bundle);
  provider.Get()->SetPolicies(policy_bundle.Pass());
  EXPECT_TRUE(provider.Get()->policies().Equals(expected_policy_bundle));

  EXPECT_CALL(delegate, PolicyProviderShutdown()).Times(1);
}

}  // namespace policy
