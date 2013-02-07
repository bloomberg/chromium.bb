// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/proxy_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace policy {

class ProxyPolicyProviderTest : public testing::Test {
 protected:
  ProxyPolicyProviderTest() {
    mock_provider_.Init();
    proxy_provider_.Init();
    proxy_provider_.AddObserver(&observer_);
  }

  virtual ~ProxyPolicyProviderTest() {
    proxy_provider_.RemoveObserver(&observer_);
    proxy_provider_.Shutdown();
    mock_provider_.Shutdown();
  }

  MockConfigurationPolicyObserver observer_;
  MockConfigurationPolicyProvider mock_provider_;
  ProxyPolicyProvider proxy_provider_;

  static scoped_ptr<PolicyBundle> CopyBundle(const PolicyBundle& bundle) {
    scoped_ptr<PolicyBundle> copy(new PolicyBundle());
    copy->CopyFrom(bundle);
    return copy.Pass();
  }

  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyProviderTest);
};

TEST_F(ProxyPolicyProviderTest, Init) {
  EXPECT_TRUE(proxy_provider_.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(proxy_provider_.policies()));
}

TEST_F(ProxyPolicyProviderTest, Delegate) {
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set("policy",
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           Value::CreateStringValue("value"));
  mock_provider_.UpdatePolicy(CopyBundle(bundle));

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(bundle.Equals(proxy_provider_.policies()));

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set("policy",
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           Value::CreateStringValue("new value"));
  mock_provider_.UpdatePolicy(CopyBundle(bundle));
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(bundle.Equals(proxy_provider_.policies()));

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(NULL);
  EXPECT_TRUE(PolicyBundle().Equals(proxy_provider_.policies()));
}

TEST_F(ProxyPolicyProviderTest, RefreshPolicies) {
  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_)).Times(0);
  EXPECT_CALL(mock_provider_, RefreshPolicies());
  proxy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  mock_provider_.UpdatePolicy(scoped_ptr<PolicyBundle>(new PolicyBundle()));
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
