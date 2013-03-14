// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/cloud/mock_user_cloud_policy_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
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
  UserCloudPolicyManagerTest()
      : store_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Set up a policy map for testing.
    policy_map_.Set("key", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    base::Value::CreateStringValue("value"));
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
    manager_.reset(
        new UserCloudPolicyManager(NULL,
                                   scoped_ptr<UserCloudPolicyStore>(store_)));
    manager_->Init();
    manager_->AddObserver(&observer_);
    Mock::VerifyAndClearExpectations(store_);
  }

  // Required by the refresh scheduler that's created by the manager.
  MessageLoop loop_;

  // Convenience policy objects.
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  MockConfigurationPolicyObserver observer_;
  MockUserCloudPolicyStore* store_;
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
