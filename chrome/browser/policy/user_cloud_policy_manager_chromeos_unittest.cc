// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/mock_cloud_policy_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::_;

namespace policy {
namespace {

class UserCloudPolicyManagerChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyManagerChromeOSTest()
      : store_(NULL) {}

  virtual void SetUp() OVERRIDE {
    chrome::RegisterLocalState(prefs_.registry());

    // Set up a policy map for testing.
    policy_map_.Set("key", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    base::Value::CreateStringValue("value"));
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);

    // Create a fake policy blob to deliver to the client.
    em::PolicyData policy_data;
    policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
    em::PolicyFetchResponse* policy_response =
        policy_blob_.mutable_policy_response()->add_response();
    ASSERT_TRUE(policy_data.SerializeToString(
        policy_response->mutable_policy_data()));

    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());
  }

  virtual void TearDown() OVERRIDE {
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
  }

  void CreateManagerWithPendingFetch() {
    store_ = new MockCloudPolicyStore();
    EXPECT_CALL(*store_, Load());
    manager_.reset(
        new UserCloudPolicyManagerChromeOS(scoped_ptr<CloudPolicyStore>(store_),
                                           true));
    manager_->Init();
    manager_->AddObserver(&observer_);
    manager_->Connect(&prefs_, &device_management_service_,
                      USER_AFFILIATION_NONE);
    Mock::VerifyAndClearExpectations(store_);
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    // Finishing the Load() operation shouldn't set the initialized flag.
    EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
    store_->NotifyStoreLoaded();
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    Mock::VerifyAndClearExpectations(&observer_);
  }

  // Required by the refresh scheduler that's created by the manager.
  MessageLoop loop_;

  // Convenience policy objects.
  em::DeviceManagementResponse policy_blob_;
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  TestingPrefServiceSimple prefs_;
  MockConfigurationPolicyObserver observer_;
  MockDeviceManagementService device_management_service_;
  MockCloudPolicyStore* store_;
  scoped_ptr<UserCloudPolicyManagerChromeOS> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSTest);
};

TEST_F(UserCloudPolicyManagerChromeOSTest, WaitForPolicyFetch) {
  CreateManagerWithPendingFetch();

  // Setting the token should trigger the policy fetch.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
  manager_->core()->client()->SetupRegistration("dm_token", "client_id");
  ASSERT_TRUE(fetch_request);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);

  // Respond to the policy fetch, which should trigger a write to |store_|.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  EXPECT_CALL(*store_, Store(_));
  fetch_request->SendResponse(DM_STATUS_SUCCESS, policy_blob_);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);

  // The load notification from |store_| should trigger the policy update and
  // flip the initialized bit.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(UserCloudPolicyManagerChromeOSTest, WaitForPolicyFetchError) {
  CreateManagerWithPendingFetch();

  // Setting the token should trigger the policy fetch.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
  manager_->core()->client()->SetupRegistration("dm_token", "client_id");
  ASSERT_TRUE(fetch_request);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);

  // Make the policy fetch fail, at which point the manager should bail out.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(AtLeast(1));
  fetch_request->SendResponse(DM_STATUS_REQUEST_FAILED, policy_blob_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(UserCloudPolicyManagerChromeOSTest, WaitForPolicyFetchCancel) {
  CreateManagerWithPendingFetch();

  // Cancelling the initial fetch should flip the flag.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  manager_->CancelWaitForPolicyFetch();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace
}  // namespace policy
