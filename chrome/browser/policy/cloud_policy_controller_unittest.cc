// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "content/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::_;
using content::BrowserThread;

class MockDeviceTokenFetcher : public DeviceTokenFetcher {
 public:
  explicit MockDeviceTokenFetcher(CloudPolicyCacheBase* cache)
      : DeviceTokenFetcher(NULL, cache, NULL, NULL) {}
  virtual ~MockDeviceTokenFetcher() {}

  MOCK_METHOD0(FetchToken, void());
  MOCK_METHOD0(SetUnmanagedState, void());
  MOCK_METHOD0(SetSerialNumberInvalidState, void());

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
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest"),
        false  /* wait_for_policy_fetch */));
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

  void CreateNewWaitingCache() {
    cache_.reset(new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest"),
        true  /* wait_for_policy_fetch */));
    // Make this cache's disk cache ready, but have it still waiting for a
    // policy fetch.
    cache_->Load();
    loop_.RunAllPending();
    ASSERT_TRUE(cache_->last_policy_refresh_time().is_null());
    ASSERT_FALSE(cache_->IsReady());
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
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

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

// If the backend reports that the device-id is already existing, the
// controller should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, DeviceIdConflict) {
  data_store_->SetupForTesting("device_token", "device_id",
                               "me@you.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceDeviceIdConflict));
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

// If the backend reports that the device has invalid serial number, the
// controller should instruct the token fetcher not to fetch a new token
// (which will in turn set and persist the correct 'sn invalid' state).
TEST_F(CloudPolicyControllerTest, InvalidSerialNumber) {
  data_store_->SetupForTesting("device_token", "device_id",
                               "who@what.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceInvalidSerialNumber));
  EXPECT_CALL(*token_fetcher_.get(), SetSerialNumberInvalidState()).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

TEST_F(CloudPolicyControllerTest, DontSetFetchingDoneWithoutTokens) {
  CreateNewWaitingCache();
  CreateNewController();
  // Initialized without an oauth token, goes into TOKEN_UNAVAILABLE state.
  // This means the controller is still waiting for an oauth token fetch.
  EXPECT_FALSE(cache_->IsReady());
}

TEST_F(CloudPolicyControllerTest, DontSetFetchingDoneWithoutFetching) {
  CreateNewWaitingCache();
  data_store_->SetupForTesting("device_token", "device_id",
                               "who@what.com", "auth", true);
  CreateNewController();
  // Initialized with an oauth token, goes into TOKEN_VALID state.
  // This means the controller has an oauth token and should fetch the next
  // token, which is the dm server register token.
  EXPECT_FALSE(cache_->IsReady());
}

TEST_F(CloudPolicyControllerTest, SetFetchingDoneForUnmanagedUsers) {
  CreateNewWaitingCache();
  data_store_->SetupForTesting("", "device_id",
                               "user@gmail.com", "auth", true);
  CreateNewController();
  loop_.RunAllPending();
  // User is in an unmanaged domain.
  EXPECT_TRUE(cache_->IsReady());
  EXPECT_TRUE(cache_->last_policy_refresh_time().is_null());
}

TEST_F(CloudPolicyControllerTest, SetFetchingDoneAfterPolicyFetch) {
  CreateNewWaitingCache();
  data_store_->SetupForTesting("device_token", "device_id",
                               "user@enterprise.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(DoAll(
      InvokeWithoutArgs(this, &CloudPolicyControllerTest::StopMessageLoop),
      MockDeviceManagementBackendSucceedSpdyCloudPolicy()));
  CreateNewController();
  loop_.RunAllPending();
  EXPECT_TRUE(cache_->IsReady());
  EXPECT_FALSE(cache_->last_policy_refresh_time().is_null());
}

TEST_F(CloudPolicyControllerTest, SetFetchingDoneAfterPolicyFetchFails) {
  CreateNewWaitingCache();
  data_store_->SetupForTesting("device_token", "device_id",
                               "user@enterprise.com", "auth", true);
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _, _)).WillOnce(DoAll(
      InvokeWithoutArgs(this, &CloudPolicyControllerTest::StopMessageLoop),
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed)));
  CreateNewController();
  loop_.RunAllPending();
  EXPECT_TRUE(cache_->IsReady());
  EXPECT_TRUE(cache_->last_policy_refresh_time().is_null());
}

}  // namespace policy
