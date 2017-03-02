// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::_;

namespace policy {

class ProfilePolicyConnectorTest : public testing::Test {
 protected:
  ProfilePolicyConnectorTest() {}
  ~ProfilePolicyConnectorTest() override {}

  void SetUp() override {
    // This must be set up before the TestingBrowserProcess is created.
    BrowserPolicyConnector::SetPolicyProviderForTesting(&mock_provider_);

    EXPECT_CALL(mock_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));

    cloud_policy_store_.NotifyStoreLoaded();
    cloud_policy_manager_.reset(new CloudPolicyManager(
        std::string(), std::string(), &cloud_policy_store_, loop_.task_runner(),
        loop_.task_runner(), loop_.task_runner()));
    cloud_policy_manager_->Init(&schema_registry_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
    cloud_policy_manager_->Shutdown();
  }

  base::MessageLoop loop_;
  SchemaRegistry schema_registry_;
  MockConfigurationPolicyProvider mock_provider_;
  MockCloudPolicyStore cloud_policy_store_;
  std::unique_ptr<CloudPolicyManager> cloud_policy_manager_;
};

TEST_F(ProfilePolicyConnectorTest, IsManagedForManagedUsers) {
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_, false);
  EXPECT_FALSE(connector.IsManaged());
  EXPECT_EQ(connector.GetManagementDomain(), "");

  cloud_policy_store_.policy_.reset(new enterprise_management::PolicyData());
  cloud_policy_store_.policy_->set_username("test@testdomain.com");
  cloud_policy_store_.policy_->set_state(
      enterprise_management::PolicyData::ACTIVE);
  EXPECT_TRUE(connector.IsManaged());
  EXPECT_EQ(connector.GetManagementDomain(), "testdomain.com");

  // Cleanup.
  connector.Shutdown();
}

TEST_F(ProfilePolicyConnectorTest, IsProfilePolicy) {
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_, false);

  // No policy is set initially.
  EXPECT_FALSE(connector.IsProfilePolicy(autofill::prefs::kAutofillEnabled));
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  EXPECT_FALSE(connector.policy_service()->GetPolicies(chrome_ns).GetValue(
      key::kAutoFillEnabled));

  // Set the policy at the cloud provider.
  cloud_policy_store_.policy_map_.Set(
      key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(false), nullptr);
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connector.IsProfilePolicy(key::kAutoFillEnabled));
  const base::Value* value =
      connector.policy_service()->GetPolicies(chrome_ns).GetValue(
          key::kAutoFillEnabled);
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::Value(false).Equals(value));

  // Now test with a higher-priority provider also setting the policy.
  PolicyMap map;
  map.Set(key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(true), nullptr);
  mock_provider_.UpdateChromePolicy(map);
  EXPECT_FALSE(connector.IsProfilePolicy(key::kAutoFillEnabled));
  value = connector.policy_service()->GetPolicies(chrome_ns).GetValue(
      key::kAutoFillEnabled);
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::Value(true).Equals(value));

  // Cleanup.
  connector.Shutdown();
}

}  // namespace policy
