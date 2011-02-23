// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/cloud_policy_cache.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestToken[] = "cloud_policy_controller_test_auth_token";

namespace policy {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;

class MockCloudPolicyIdentityStrategy : public CloudPolicyIdentityStrategy {
 public:
  MockCloudPolicyIdentityStrategy() {}
  virtual ~MockCloudPolicyIdentityStrategy() {}

  MOCK_METHOD0(GetDeviceToken, std::string());
  MOCK_METHOD0(GetDeviceID, std::string());
  MOCK_METHOD2(GetCredentials, bool(std::string*, std::string*));
  virtual void OnDeviceTokenAvailable(const std::string&) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyIdentityStrategy);
};

ACTION_P2(MockCloudPolicyIdentityStrategyGetCredentials, username, auth_token) {
  *arg0 = username;
  *arg1 = auth_token;
  return true;
}

class MockDeviceTokenFetcher : public DeviceTokenFetcher {
 public:
  explicit MockDeviceTokenFetcher(CloudPolicyCache* cache)
      : DeviceTokenFetcher(NULL, cache) {}
  virtual ~MockDeviceTokenFetcher() {}

  MOCK_METHOD0(GetDeviceToken, std::string&());
  MOCK_METHOD2(FetchToken, void(const std::string&, const std::string&));

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
    cache_.reset(new CloudPolicyCache(
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest")));
    token_fetcher_.reset(new MockDeviceTokenFetcher(cache_.get()));
  }

  virtual void TearDown() {
    controller_.reset();  // Unregisters observers.
  }

  // Takes ownership of |backend|.
  void CreateNewController(DeviceManagementBackend* backend) {
    controller_.reset(new CloudPolicyController(
        cache_.get(), backend, token_fetcher_.get(), &identity_strategy_));
  }

  void CreateNewController(DeviceManagementBackend* backend,
                           int64 policy_refresh_rate_ms,
                           int policy_refresh_deviation_factor_percent,
                           int64 policy_refresh_deviation_max_ms,
                           int64 policy_refresh_error_delay_ms) {
    controller_.reset(new CloudPolicyController(
        cache_.get(), backend, token_fetcher_.get(), &identity_strategy_,
        policy_refresh_rate_ms,
        policy_refresh_deviation_factor_percent,
        policy_refresh_deviation_max_ms,
        policy_refresh_error_delay_ms));
  }

  void ExpectHasSpdyPolicy() {
    MockConfigurationPolicyStore store;
    EXPECT_CALL(store, Apply(_, _)).Times(AtLeast(1));
    cache_->GetManagedPolicyProvider()->Provide(&store);
    FundamentalValue expected(true);
    ASSERT_TRUE(store.Get(kPolicyDisableSpdy) != NULL);
    EXPECT_TRUE(store.Get(kPolicyDisableSpdy)->Equals(&expected));
  }

  void SetupIdentityStrategy(const std::string& device_token,
                             const std::string& device_id,
                             const std::string& user_name,
                             const std::string& auth_token) {
    EXPECT_CALL(identity_strategy_, GetDeviceToken()).WillRepeatedly(
        Return(device_token));
    EXPECT_CALL(identity_strategy_, GetDeviceID()).WillRepeatedly(
        Return(device_id));
    if (!user_name.empty()) {
      EXPECT_CALL(identity_strategy_, GetCredentials(_, _)).WillRepeatedly(
          MockCloudPolicyIdentityStrategyGetCredentials(user_name, auth_token));
    }
  }

 protected:
  scoped_ptr<CloudPolicyCache> cache_;
  scoped_ptr<CloudPolicyController> controller_;
  scoped_ptr<MockDeviceTokenFetcher> token_fetcher_;
  MockCloudPolicyIdentityStrategy identity_strategy_;
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
  SetupIdentityStrategy("fake_device_token", "device_id", "", "");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  CreateNewController(backend);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If no device token is present when the controller starts up, it should
// instruct the token_fetcher_ to fetch one.
TEST_F(CloudPolicyControllerTest, StartupWithoutDeviceToken) {
  SetupIdentityStrategy("", "device_id", "a@b.com", "auth_token");
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _)).Times(1);
  CreateNewController(NULL);
  loop_.RunAllPending();
}

// If the current user belongs to a known non-managed domain, no token fetch
// should be initiated.
TEST_F(CloudPolicyControllerTest, StartupUnmanagedUser) {
  SetupIdentityStrategy("", "device_id", "DannoHelper@gmail.com", "auth_token");
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _)).Times(0);
  CreateNewController(NULL);
  loop_.RunAllPending();
}

// After policy has been fetched successfully, a new fetch should be triggered
// after the refresh interval has timed out.
TEST_F(CloudPolicyControllerTest, RefreshAfterSuccessfulPolicy) {
  SetupIdentityStrategy("device_token", "device_id",
                        "DannoHelperDelegate@b.com", "auth_token");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy()).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed));
  CreateNewController(backend, 0, 0, 0, 1000 * 1000);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If poliy fetching failed, it should be retried.
TEST_F(CloudPolicyControllerTest, RefreshAfterError) {
  SetupIdentityStrategy("device_token", "device_id",
                        "DannoHelperDelegateImpl@b.com", "auth_token");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  CreateNewController(backend, 1000 * 1000, 0, 0, 0);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If the backend reports that the device token was invalid, the controller
// should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, InvalidToken) {
  SetupIdentityStrategy("device_token", "device_id", "standup@ten.am", "auth");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementTokenInvalid));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _)).Times(1);
  CreateNewController(backend);
  loop_.RunAllPending();
}

// If the backend reports that the device is unknown to the server, the
// controller should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, DeviceNotFound) {
  SetupIdentityStrategy("device_token", "device_id", "me@you.com", "auth");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceDeviceNotFound));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _)).Times(1);
  CreateNewController(backend);
  loop_.RunAllPending();
}

// If the backend reports that the device is no longer managed, the controller
// shoud instruct the token fetcher to fetch a new token (which will in turn
// set and persist the correct 'unmanaged' state).
TEST_F(CloudPolicyControllerTest, NoLongerManaged) {
  SetupIdentityStrategy("device_token", "device_id", "who@what.com", "auth");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementNotSupported));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _)).Times(1);
  CreateNewController(backend);
  loop_.RunAllPending();
}

// If the server doesn't support the new protocol, the controller should fall
// back to the old protocol.
TEST_F(CloudPolicyControllerTest, FallbackToOldProtocol) {
  SetupIdentityStrategy("device_token", "device_id", "a@b.com", "auth");
  MockDeviceManagementBackend* backend = new MockDeviceManagementBackend();
  // If the CloudPolicyRequest fails with kErrorRequestInvalid...
  EXPECT_CALL(*backend, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestInvalid));
  // ...the client should fall back to a classic PolicyRequest,
  // and remember this fallback for any future request,
  // both after successful fetches and after errors.
  EXPECT_CALL(*backend, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedBooleanPolicy(
          key::kDisableSpdy, true)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorHttpStatus)).WillOnce(
      Return());
  CreateNewController(backend, 0, 0, 0, 0);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

}  // namespace policy
