// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "content/browser/browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

class MockDeviceTokenFetcher : public DeviceTokenFetcher {
 public:
  explicit MockDeviceTokenFetcher(CloudPolicyCacheBase* cache)
      : DeviceTokenFetcher(NULL, cache, NULL, NULL) {}
  virtual ~MockDeviceTokenFetcher() {}

  MOCK_METHOD0(FetchToken, void());
  MOCK_METHOD0(SetUnmanagedState, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceTokenFetcher);
};

class CloudPolicyControllerTest : public testing::Test {
 public:
  CloudPolicyControllerTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual ~CloudPolicyControllerTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    cache_.reset(new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest")));
    token_fetcher_.reset(new MockDeviceTokenFetcher(cache_.get()));
    EXPECT_CALL(service_, CreateBackend())
        .Times(AnyNumber())
        .WillRepeatedly(MockDeviceManagementServiceProxyBackend(&backend_));
    data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
  }

  virtual void TearDown() {
    controller_.reset();  // Unregisters observers.
    data_store_.reset();
  }

  void CreateNewController() {
    controller_.reset(new CloudPolicyController(
        &service_, cache_.get(), token_fetcher_.get(), data_store_.get(),
        &notifier_, new DummyWorkScheduler));
  }

  void ExpectHasSpdyPolicy() {
    base::FundamentalValue expected(true);
    const PolicyMap* policy_map = cache_->policy(
        CloudPolicyCacheBase::POLICY_LEVEL_MANDATORY);
    ASSERT_TRUE(Value::Equals(&expected, policy_map->Get(kPolicyDisableSpdy)));
  }

  void StopMessageLoop() {
    loop_.QuitNow();
  }

 protected:
  scoped_ptr<CloudPolicyCacheBase> cache_;
  scoped_ptr<CloudPolicyController> controller_;
  scoped_ptr<MockDeviceTokenFetcher> token_fetcher_;
  scoped_ptr<CloudPolicyDataStore> data_store_;
  MockDeviceManagementBackend backend_;
  MockDeviceManagementService service_;
  PolicyNotifier notifier_;
  ScopedTempDir temp_user_data_dir_;
  MessageLoop loop_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyControllerTest);
};

// If a device token is present when the controller starts up, it should
// fetch and apply policy.
TEST_F(CloudPolicyControllerTest, StartupWithDeviceToken) {
  data_store_->SetupForTesting("fake_device_token", "device_id", "", "",
                                true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(DoAll(
      InvokeWithoutArgs(this, &CloudPolicyControllerTest::StopMessageLoop),
      MockDeviceManagementBackendSucceedSpdyCloudPolicy()));
  CreateNewController();
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If no device token is present when the controller starts up, it should
// instruct the token_fetcher_ to fetch one.
TEST_F(CloudPolicyControllerTest, StartupWithoutDeviceToken) {
  data_store_->SetupForTesting("", "device_id", "a@b.com", "auth_token",
                                true);
  EXPECT_CALL(*token_fetcher_.get(), FetchToken()).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

// If the current user belongs to a known non-managed domain, no token fetch
// should be initiated.
TEST_F(CloudPolicyControllerTest, StartupUnmanagedUser) {
  data_store_->SetupForTesting("", "device_id", "DannoHelper@gmail.com",
                                "auth_token", true);
  EXPECT_CALL(*token_fetcher_.get(), FetchToken()).Times(0);
  CreateNewController();
  loop_.RunAllPending();
}

// After policy has been fetched successfully, a new fetch should be triggered
// after the refresh interval has timed out.
TEST_F(CloudPolicyControllerTest, RefreshAfterSuccessfulPolicy) {
  data_store_->SetupForTesting("device_token", "device_id",
                                "DannoHelperDelegate@b.com",
                                "auth_token", true);
  {
    InSequence s;
    EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(DoAll(
        InvokeWithoutArgs(this, &CloudPolicyControllerTest::StopMessageLoop),
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestFailed)));
  }
  CreateNewController();
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If policy fetching failed, it should be retried.
TEST_F(CloudPolicyControllerTest, RefreshAfterError) {
  data_store_->SetupForTesting("device_token", "device_id",
                                "DannoHelperDelegateImpl@b.com",
                                "auth_token", true);
  {
    InSequence s;
    EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestFailed));
    EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(DoAll(
        InvokeWithoutArgs(this,
                          &CloudPolicyControllerTest::StopMessageLoop),
        MockDeviceManagementBackendSucceedSpdyCloudPolicy()));
  }
  CreateNewController();
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If the backend reports that the device token was invalid, the controller
// should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, InvalidToken) {
  data_store_->SetupForTesting("device_token", "device_id",
                                "standup@ten.am", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementTokenInvalid));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken()).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

// If the backend reports that the device is unknown to the server, the
// controller should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, DeviceNotFound) {
  data_store_->SetupForTesting("device_token", "device_id",
                                "me@you.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceDeviceNotFound));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken()).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

// If the backend reports that the device is no longer managed, the controller
// should instruct the token fetcher to fetch a new token (which will in turn
// set and persist the correct 'unmanaged' state).
TEST_F(CloudPolicyControllerTest, NoLongerManaged) {
  data_store_->SetupForTesting("device_token", "device_id",
                                "who@what.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementNotSupported));
  EXPECT_CALL(*token_fetcher_.get(), SetUnmanagedState()).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

}  // namespace policy
