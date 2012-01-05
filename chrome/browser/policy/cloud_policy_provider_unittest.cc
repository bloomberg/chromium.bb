// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_provider.h"

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_provider_impl.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

class MockCloudPolicyCache : public CloudPolicyCacheBase {
 public:
  MockCloudPolicyCache() {}
  virtual ~MockCloudPolicyCache() {}

  // CloudPolicyCacheBase implementation.
  void Load() OVERRIDE {}
  void SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE {}
  bool DecodePolicyData(const em::PolicyData& policy_data,
                        PolicyMap* mandatory,
                        PolicyMap* recommended) OVERRIDE {
    return true;
  }

  void SetUnmanaged() OVERRIDE {
    is_unmanaged_ = true;
  }

  // Non-const accessors for underlying PolicyMaps.
  PolicyMap* raw_mandatory_policy() {
    return &mandatory_policy_;
  }

  PolicyMap* raw_recommended_policy() {
    return &recommended_policy_;
  }

  void set_initialized(bool initialized) {
    initialization_complete_ = initialized;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyCache);
};

class CloudPolicyProviderTest : public testing::Test {
 protected:
  void CreateCloudPolicyProvider() {
    cloud_policy_provider_.reset(
        new CloudPolicyProviderImpl(
            &browser_policy_connector_,
            GetChromePolicyDefinitionList(),
            CloudPolicyCacheBase::POLICY_LEVEL_MANDATORY));
  }

  // Appends the caches to a provider and then provides the policies to
  // |result|.
  void RunCachesThroughProvider(MockCloudPolicyCache caches[], int n,
                                PolicyMap* result) {
    CloudPolicyProviderImpl provider(
        &browser_policy_connector_,
        GetChromePolicyDefinitionList(),
        CloudPolicyCacheBase::POLICY_LEVEL_MANDATORY);
    for (int i = 0; i < n; i++) {
      provider.AppendCache(&caches[i]);
    }
    provider.Provide(result);
  }

  void CombineTwoPolicyMaps(const PolicyMap& base,
                            const PolicyMap& overlay,
                            PolicyMap* out_map) {
    MockCloudPolicyCache caches[2];
    caches[0].raw_mandatory_policy()->CopyFrom(base);
    caches[0].set_initialized(true);
    caches[1].raw_mandatory_policy()->CopyFrom(overlay);
    caches[1].set_initialized(true);
    RunCachesThroughProvider(caches, 2, out_map);
  }

  void FixDeprecatedPolicies(PolicyMap* policies) {
    CloudPolicyProviderImpl::FixDeprecatedPolicies(policies);
  }

  scoped_ptr<CloudPolicyProviderImpl> cloud_policy_provider_;

 private:
  BrowserPolicyConnector browser_policy_connector_;
};

// Proxy setting distributed over multiple caches.
TEST_F(CloudPolicyProviderTest,
       ProxySettingDistributedOverMultipleCaches) {
  // There are proxy_policy_count()+1 = 6 caches and they are mixed together by
  // one instance of CloudPolicyProvider. The first cache has some policies but
  // no proxy-related ones. The following caches have each one proxy-policy set.
  const int n = 6;
  MockCloudPolicyCache caches[n];

  // Prepare |cache[0]| to serve some non-proxy policies.
  caches[0].raw_mandatory_policy()->Set(kPolicyShowHomeButton,
                                        Value::CreateBooleanValue(true));
  caches[0].raw_mandatory_policy()->Set(kPolicyIncognitoEnabled,
                                        Value::CreateBooleanValue(true));
  caches[0].raw_mandatory_policy()->Set(kPolicyTranslateEnabled,
                                        Value::CreateBooleanValue(true));
  caches[0].set_initialized(true);

  // Prepare the other caches to serve one proxy-policy each.
  caches[1].raw_mandatory_policy()->Set(kPolicyProxyMode,
                                        Value::CreateStringValue("cache 1"));
  caches[1].set_initialized(true);
  caches[2].raw_mandatory_policy()->Set(kPolicyProxyServerMode,
                                        Value::CreateIntegerValue(2));
  caches[2].set_initialized(true);
  caches[3].raw_mandatory_policy()->Set(kPolicyProxyServer,
                                        Value::CreateStringValue("cache 3"));
  caches[3].set_initialized(true);
  caches[4].raw_mandatory_policy()->Set(kPolicyProxyPacUrl,
                                        Value::CreateStringValue("cache 4"));
  caches[4].set_initialized(true);
  caches[5].raw_mandatory_policy()->Set(kPolicyProxyMode,
                                        Value::CreateStringValue("cache 5"));
  caches[5].set_initialized(true);

  PolicyMap policies;
  RunCachesThroughProvider(caches, n, &policies);

  // Verify expectations.
  EXPECT_TRUE(policies.Get(kPolicyProxyMode) == NULL);
  EXPECT_TRUE(policies.Get(kPolicyProxyServerMode) == NULL);
  EXPECT_TRUE(policies.Get(kPolicyProxyServer) == NULL);
  EXPECT_TRUE(policies.Get(kPolicyProxyPacUrl) == NULL);

  const Value* value = policies.Get(kPolicyProxySettings);
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* settings = static_cast<const DictionaryValue*>(value);
  std::string mode;
  EXPECT_TRUE(settings->GetString(GetPolicyName(kPolicyProxyMode), &mode));
  EXPECT_EQ("cache 1", mode);

  base::FundamentalValue expected(true);
  EXPECT_TRUE(base::Value::Equals(&expected,
                                  policies.Get(kPolicyShowHomeButton)));
  EXPECT_TRUE(base::Value::Equals(&expected,
                                  policies.Get(kPolicyIncognitoEnabled)));
  EXPECT_TRUE(base::Value::Equals(&expected,
                                  policies.Get(kPolicyTranslateEnabled)));
}

// Combining two PolicyMaps.
TEST_F(CloudPolicyProviderTest, CombineTwoPolicyMapsSame) {
  PolicyMap A, B, C;
  A.Set(kPolicyHomepageLocation,
        Value::CreateStringValue("http://www.chromium.org"));
  B.Set(kPolicyHomepageLocation,
        Value::CreateStringValue("http://www.google.com"));
  A.Set(kPolicyApplicationLocaleValue, Value::CreateStringValue("hu"));
  B.Set(kPolicyApplicationLocaleValue, Value::CreateStringValue("us"));
  A.Set(kPolicyDevicePolicyRefreshRate, new base::FundamentalValue(100));
  B.Set(kPolicyDevicePolicyRefreshRate, new base::FundamentalValue(200));
  CombineTwoPolicyMaps(A, B, &C);
  EXPECT_TRUE(A.Equals(C));
}

TEST_F(CloudPolicyProviderTest, CombineTwoPolicyMapsEmpty) {
  PolicyMap A, B, C;
  CombineTwoPolicyMaps(A, B, &C);
  EXPECT_TRUE(C.empty());
}

TEST_F(CloudPolicyProviderTest, CombineTwoPolicyMapsPartial) {
  PolicyMap A, B, C;

  A.Set(kPolicyHomepageLocation,
        Value::CreateStringValue("http://www.chromium.org"));
  B.Set(kPolicyHomepageLocation,
        Value::CreateStringValue("http://www.google.com"));
  B.Set(kPolicyApplicationLocaleValue, Value::CreateStringValue("us"));
  A.Set(kPolicyDevicePolicyRefreshRate, new base::FundamentalValue(100));
  B.Set(kPolicyDevicePolicyRefreshRate, new base::FundamentalValue(200));
  CombineTwoPolicyMaps(A, B, &C);

  const Value* value;
  std::string string_value;
  int int_value;
  value = C.Get(kPolicyHomepageLocation);
  ASSERT_TRUE(NULL != value);
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ("http://www.chromium.org", string_value);
  value = C.Get(kPolicyApplicationLocaleValue);
  ASSERT_TRUE(NULL != value);
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ("us", string_value);
  value = C.Get(kPolicyDevicePolicyRefreshRate);
  ASSERT_TRUE(NULL != value);
  EXPECT_TRUE(value->GetAsInteger(&int_value));
  EXPECT_EQ(100, int_value);
}

TEST_F(CloudPolicyProviderTest, CombineTwoPolicyMapsProxies) {
  const int a_value = 1;
  const int b_value = -1;
  PolicyMap A, B, C;

  A.Set(kPolicyProxyMode, Value::CreateIntegerValue(a_value));

  B.Set(kPolicyProxyServerMode, Value::CreateIntegerValue(b_value));
  B.Set(kPolicyProxyServer, Value::CreateIntegerValue(b_value));
  B.Set(kPolicyProxyPacUrl, Value::CreateIntegerValue(b_value));
  B.Set(kPolicyProxyBypassList, Value::CreateIntegerValue(b_value));

  CombineTwoPolicyMaps(A, B, &C);

  FixDeprecatedPolicies(&A);
  FixDeprecatedPolicies(&B);
  EXPECT_TRUE(A.Equals(C));
  EXPECT_FALSE(B.Equals(C));
}

TEST_F(CloudPolicyProviderTest, RefreshPolicies) {
  CreateCloudPolicyProvider();
  MockCloudPolicyCache cache0;
  MockCloudPolicyCache cache1;
  MockCloudPolicyCache cache2;
  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(cloud_policy_provider_.get(), &observer);

  // OnUpdatePolicy is called when the provider doesn't have any caches.
  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(1);
  cloud_policy_provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  // OnUpdatePolicy is called when all the caches have updated.
  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(2);
  cloud_policy_provider_->AppendCache(&cache0);
  cloud_policy_provider_->AppendCache(&cache1);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  cloud_policy_provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  // Updating just one of the caches is not enough.
  cloud_policy_provider_->OnCacheUpdate(&cache0);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  // This cache wasn't available when RefreshPolicies was called, so it isn't
  // required to fire the update.
  cloud_policy_provider_->AppendCache(&cache2);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(1);
  cloud_policy_provider_->OnCacheUpdate(&cache1);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  cloud_policy_provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  cloud_policy_provider_->OnCacheUpdate(&cache0);
  cloud_policy_provider_->OnCacheUpdate(&cache1);
  Mock::VerifyAndClearExpectations(&observer);

  // If a cache refreshes more than once, the provider should still wait for
  // the others before firing the update.
  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(0);
  cloud_policy_provider_->OnCacheUpdate(&cache0);
  Mock::VerifyAndClearExpectations(&observer);

  // Fire updates if one of the required caches goes away while waiting.
  EXPECT_CALL(observer, OnUpdatePolicy(cloud_policy_provider_.get())).Times(1);
  cloud_policy_provider_->OnCacheGoingAway(&cache2);
  Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace policy
