// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "content/browser/browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestToken[] = "cloud_policy_controller_test_auth_token";

namespace policy {

namespace em = enterprise_management;

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
  MOCK_METHOD0(GetMachineID, std::string());
  MOCK_METHOD0(GetMachineModel, std::string());
  MOCK_METHOD0(GetPolicyType, std::string());
  MOCK_METHOD0(GetPolicyRegisterType, em::DeviceRegisterRequest_Type());

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
  explicit MockDeviceTokenFetcher(CloudPolicyCacheBase* cache)
      : DeviceTokenFetcher(NULL, cache, NULL) {}
  virtual ~MockDeviceTokenFetcher() {}

  MOCK_METHOD0(GetDeviceToken, const std::string&());
  MOCK_METHOD5(FetchToken,
      void(const std::string&, const std::string&,
           em::DeviceRegisterRequest_Type,
           const std::string&, const std::string&));
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
    service_.set_backend(&backend_);
  }

  virtual void TearDown() {
    controller_.reset();  // Unregisters observers.
  }

  // Takes ownership of |backend|.
  void CreateNewController() {
    controller_.reset(new CloudPolicyController(
        &service_, cache_.get(), token_fetcher_.get(), &identity_strategy_,
        &notifier_));
  }

  void CreateNewController(int64 policy_refresh_rate_ms,
                           int policy_refresh_deviation_factor_percent,
                           int64 policy_refresh_deviation_max_ms,
                           int64 policy_refresh_error_delay_ms) {
    controller_.reset(new CloudPolicyController(
        &service_, cache_.get(), token_fetcher_.get(), &identity_strategy_,
        &notifier_,
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

  void SetupIdentityStrategy(
      const std::string& device_token,
      const std::string& device_id,
      const std::string& machine_id,
      const std::string& machine_model,
      const std::string& policy_type,
      const em::DeviceRegisterRequest_Type& policy_register_type,
      const std::string& user_name,
      const std::string& auth_token) {
    EXPECT_CALL(identity_strategy_, GetDeviceToken()).WillRepeatedly(
        Return(device_token));
    EXPECT_CALL(identity_strategy_, GetDeviceID()).WillRepeatedly(
        Return(device_id));
    EXPECT_CALL(identity_strategy_, GetMachineID()).WillRepeatedly(
        Return(machine_id));
    EXPECT_CALL(identity_strategy_, GetMachineModel()).WillRepeatedly(
        Return(machine_model));
    EXPECT_CALL(identity_strategy_, GetPolicyType()).WillRepeatedly(
        Return(policy_type));
    EXPECT_CALL(identity_strategy_, GetPolicyRegisterType()).WillRepeatedly(
        Return(policy_register_type));
    if (!user_name.empty()) {
      EXPECT_CALL(identity_strategy_, GetCredentials(_, _)).WillRepeatedly(
          MockCloudPolicyIdentityStrategyGetCredentials(user_name, auth_token));
    }
  }

 protected:
  scoped_ptr<CloudPolicyCacheBase> cache_;
  scoped_ptr<CloudPolicyController> controller_;
  scoped_ptr<MockDeviceTokenFetcher> token_fetcher_;
  MockCloudPolicyIdentityStrategy identity_strategy_;
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
  SetupIdentityStrategy("fake_device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER, "", "");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  CreateNewController();
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If no device token is present when the controller starts up, it should
// instruct the token_fetcher_ to fetch one.
TEST_F(CloudPolicyControllerTest, StartupWithoutDeviceToken) {
  SetupIdentityStrategy("", "device_id", "machine_id", "machine_model",
                        "google/chromeos/user", em::DeviceRegisterRequest::USER,
                        "a@b.com", "auth_token");
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _, _, _, _)).Times(1);
  CreateNewController();
  loop_.RunAllPending();
}

// If the current user belongs to a known non-managed domain, no token fetch
// should be initiated.
TEST_F(CloudPolicyControllerTest, StartupUnmanagedUser) {
  SetupIdentityStrategy("", "device_id",  "machine_id", "machine_mode",
                        "google/chromeos/user", em::DeviceRegisterRequest::USER,
                        "DannoHelper@gmail.com", "auth_token");
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _, _, _, _)).Times(0);
  CreateNewController();
  loop_.RunAllPending();
}

// After policy has been fetched successfully, a new fetch should be triggered
// after the refresh interval has timed out.
TEST_F(CloudPolicyControllerTest, RefreshAfterSuccessfulPolicy) {
  SetupIdentityStrategy("device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER,
                        "DannoHelperDelegate@b.com", "auth_token");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy()).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed));
  CreateNewController(0, 0, 0, 1000 * 1000);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If policy fetching failed, it should be retried.
TEST_F(CloudPolicyControllerTest, RefreshAfterError) {
  SetupIdentityStrategy("device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER,
                        "DannoHelperDelegateImpl@b.com", "auth_token");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  CreateNewController(1000 * 1000, 0, 0, 0);
  loop_.RunAllPending();
  ExpectHasSpdyPolicy();
}

// If the backend reports that the device token was invalid, the controller
// should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, InvalidToken) {
  SetupIdentityStrategy("device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER,
                        "standup@ten.am", "auth");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementTokenInvalid));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _, _, _, _)).Times(1);
  CreateNewController(1000 * 1000, 0, 0, 0);
  loop_.RunAllPending();
}

// If the backend reports that the device is unknown to the server, the
// controller should instruct the token fetcher to fetch a new token.
TEST_F(CloudPolicyControllerTest, DeviceNotFound) {
  SetupIdentityStrategy("device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER,
                        "me@you.com", "auth");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceDeviceNotFound));
  EXPECT_CALL(*token_fetcher_.get(), FetchToken(_, _, _, _, _)).Times(1);
  CreateNewController(1000 * 1000, 0, 0, 0);
  loop_.RunAllPending();
}

// If the backend reports that the device is no longer managed, the controller
// should instruct the token fetcher to fetch a new token (which will in turn
// set and persist the correct 'unmanaged' state).
TEST_F(CloudPolicyControllerTest, NoLongerManaged) {
  SetupIdentityStrategy("device_token", "device_id", "machine_id",
                        "machine_model", "google/chromeos/user",
                        em::DeviceRegisterRequest::USER,
                        "who@what.com", "auth");
  EXPECT_CALL(backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorServiceManagementNotSupported));
  EXPECT_CALL(*token_fetcher_.get(), SetUnmanagedState()).Times(1);
  CreateNewController(0, 0, 0, 1000 * 1000);
  loop_.RunAllPending();
}

}  // namespace policy
