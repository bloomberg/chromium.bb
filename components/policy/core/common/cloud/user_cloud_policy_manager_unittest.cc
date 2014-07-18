// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/schema_registry.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::_;

namespace policy {
namespace {

class UserCloudPolicyManagerTest : public testing::Test {
 protected:
  UserCloudPolicyManagerTest() : store_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Set up a policy map for testing.
    policy_map_.Set("key",
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER,
                    new base::StringValue("value"),
                    NULL);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);
  }

  virtual void TearDown() OVERRIDE {
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
  }

  void CreateManager() {
    store_ = new MockUserCloudPolicyStore();
    EXPECT_CALL(*store_, Load());
    manager_.reset(new UserCloudPolicyManager(
        scoped_ptr<UserCloudPolicyStore>(store_),
        base::FilePath(),
        scoped_ptr<CloudExternalDataManager>(),
        loop_.message_loop_proxy(),
        loop_.message_loop_proxy(),
        loop_.message_loop_proxy()));
    manager_->Init(&schema_registry_);
    manager_->AddObserver(&observer_);
    Mock::VerifyAndClearExpectations(store_);
  }

  // Required by the refresh scheduler that's created by the manager.
  base::MessageLoop loop_;

  // Convenience policy objects.
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  MockUserCloudPolicyStore* store_;  // Not owned.
  scoped_ptr<UserCloudPolicyManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerTest);
};

TEST_F(UserCloudPolicyManagerTest, DisconnectAndRemovePolicy) {
  // Load policy, make sure it goes away when DisconnectAndRemovePolicy() is
  // called.
  CreateManager();
  store_->policy_map_.CopyFrom(policy_map_);
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(*store_, Clear());
  manager_->DisconnectAndRemovePolicy();
  EXPECT_FALSE(manager_->core()->service());
}

}  // namespace
}  // namespace policy
