// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_provider.h"

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "content/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace em = enterprise_management;

namespace policy {

class MockCloudPolicyCache : public CloudPolicyCacheBase {
 public:
  MockCloudPolicyCache() {}
  virtual ~MockCloudPolicyCache() {}

  virtual void Load() OVERRIDE {}

  virtual bool SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE {
    return true;
  }

  virtual void SetUnmanaged() OVERRIDE {
    is_unmanaged_ = true;
  }

  virtual bool DecodePolicyData(const em::PolicyData& policy_data,
                                PolicyMap* policies) OVERRIDE {
    return true;
  }

  PolicyMap* mutable_policy() {
    return &policies_;
  }

  // Make these methods public.
  using CloudPolicyCacheBase::SetReady;
  using CloudPolicyCacheBase::NotifyObservers;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyCache);
};

class CloudPolicyProviderTest : public testing::Test {
 protected:
  CloudPolicyProviderTest()
      : cloud_policy_provider_(&browser_policy_connector_,
                               GetChromePolicyDefinitionList(),
                               POLICY_LEVEL_MANDATORY) {}

  void SetUp() OVERRIDE {
    registrar_.Init(&cloud_policy_provider_, &observer_);
  }

  void AddUserCache() {
    EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
    cloud_policy_provider_.SetUserPolicyCache(&user_policy_cache_);
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void AddDeviceCache() {
#if defined(OS_CHROMEOS)
    EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
    cloud_policy_provider_.SetDevicePolicyCache(&device_policy_cache_);
    Mock::VerifyAndClearExpectations(&observer_);
#endif
  }

  void SetUserCacheReady() {
    EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
    user_policy_cache_.SetReady();
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_TRUE(user_policy_cache_.IsReady());
  }

  void SetDeviceCacheReady() {
#if defined(OS_CHROMEOS)
    EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
    device_policy_cache_.SetReady();
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_TRUE(device_policy_cache_.IsReady());
#endif
  }

  BrowserPolicyConnector browser_policy_connector_;

  MockCloudPolicyCache user_policy_cache_;
#if defined(OS_CHROMEOS)
  MockCloudPolicyCache device_policy_cache_;
#endif

  CloudPolicyProvider cloud_policy_provider_;
  MockConfigurationPolicyObserver observer_;
  ConfigurationPolicyObserverRegistrar registrar_;
};

TEST_F(CloudPolicyProviderTest, Initialization) {
  EXPECT_FALSE(cloud_policy_provider_.IsInitializationComplete());

  // The provider only becomes initialized when it has all caches, and the
  // caches are ready too.
  AddDeviceCache();
  EXPECT_FALSE(cloud_policy_provider_.IsInitializationComplete());
  SetDeviceCacheReady();
  EXPECT_FALSE(cloud_policy_provider_.IsInitializationComplete());
  AddUserCache();
  EXPECT_FALSE(user_policy_cache_.IsReady());
  EXPECT_FALSE(cloud_policy_provider_.IsInitializationComplete());

  PolicyMap policy;
  EXPECT_TRUE(cloud_policy_provider_.Provide(&policy));
  EXPECT_TRUE(policy.empty());

  SetUserCacheReady();
  EXPECT_TRUE(cloud_policy_provider_.IsInitializationComplete());
  EXPECT_TRUE(cloud_policy_provider_.Provide(&policy));
  EXPECT_TRUE(policy.empty());

  const std::string kUrl("http://chromium.org");
  user_policy_cache_.mutable_policy()->Set(key::kHomepageLocation,
                                           POLICY_LEVEL_MANDATORY,
                                           POLICY_SCOPE_USER,
                                           Value::CreateStringValue(kUrl));
  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
  user_policy_cache_.NotifyObservers();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(cloud_policy_provider_.Provide(&policy));

  PolicyMap expected;
  expected.Set(key::kHomepageLocation,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               Value::CreateStringValue(kUrl));
  EXPECT_TRUE(policy.Equals(expected));
}

TEST_F(CloudPolicyProviderTest, RefreshPolicies) {
  // OnUpdatePolicy is called when the provider doesn't have any caches.
  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(1);
  cloud_policy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);

  // OnUpdatePolicy is called when all the caches have updated.
  AddUserCache();

  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(0);
  cloud_policy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(1);
  cloud_policy_provider_.OnCacheUpdate(&user_policy_cache_);
  Mock::VerifyAndClearExpectations(&observer_);

#if defined(OS_CHROMEOS)
  AddDeviceCache();

  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(0);
  cloud_policy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);

  // Updating one of the caches is not enough, both must be updated.
  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(0);
  cloud_policy_provider_.OnCacheUpdate(&user_policy_cache_);
  Mock::VerifyAndClearExpectations(&observer_);

  // If a cache refreshes more than once, the provider should still wait for
  // the others before firing the update.
  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(0);
  cloud_policy_provider_.OnCacheUpdate(&user_policy_cache_);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_)).Times(1);
  cloud_policy_provider_.OnCacheUpdate(&device_policy_cache_);
  Mock::VerifyAndClearExpectations(&observer_);
#endif
}

#if defined(OS_CHROMEOS)
TEST_F(CloudPolicyProviderTest, MergeProxyPolicies) {
  AddDeviceCache();
  AddUserCache();
  SetDeviceCacheReady();
  SetUserCacheReady();
  EXPECT_TRUE(cloud_policy_provider_.IsInitializationComplete());
  PolicyMap policy;
  EXPECT_TRUE(cloud_policy_provider_.Provide(&policy));
  EXPECT_TRUE(policy.empty());

  // User policy takes precedence over device policy.
  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
  device_policy_cache_.mutable_policy()->Set(
      key::kProxyMode, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
      Value::CreateStringValue("device mode"));
  device_policy_cache_.NotifyObservers();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&cloud_policy_provider_));
  user_policy_cache_.mutable_policy()->Set(
      key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      Value::CreateStringValue("user mode"));
  user_policy_cache_.NotifyObservers();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(cloud_policy_provider_.Provide(&policy));

  // The deprecated proxy policies are converted to the new ProxySettings.
  EXPECT_TRUE(policy.Get(key::kProxyMode) == NULL);
  EXPECT_TRUE(policy.Get(key::kProxyServerMode) == NULL);
  EXPECT_TRUE(policy.Get(key::kProxyServer) == NULL);
  EXPECT_TRUE(policy.Get(key::kProxyPacUrl) == NULL);

  EXPECT_EQ(1u, policy.size());
  const PolicyMap::Entry* entry = policy.Get(key::kProxySettings);
  ASSERT_TRUE(entry != NULL);
  ASSERT_TRUE(entry->value != NULL);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  const DictionaryValue* settings;
  ASSERT_TRUE(entry->value->GetAsDictionary(&settings));
  std::string mode;
  EXPECT_TRUE(settings->GetString(key::kProxyMode, &mode));
  EXPECT_EQ("user mode", mode);
}
#endif

}  // namespace policy
