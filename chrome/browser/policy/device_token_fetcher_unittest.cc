// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

const char kTestToken[] = "device_token_fetcher_test_auth_token";

using testing::_;
using testing::Mock;

class MockTokenAvailableObserver : public CloudPolicyDataStore::Observer {
 public:
  MockTokenAvailableObserver() {}
  virtual ~MockTokenAvailableObserver() {}

  MOCK_METHOD0(OnDeviceTokenChanged, void());
  MOCK_METHOD0(OnCredentialsChanged, void());
  MOCK_METHOD0(OnDataStoreGoingAway, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTokenAvailableObserver);
};

class DeviceTokenFetcherTest : public testing::Test {
 protected:
  DeviceTokenFetcherTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
  }

  virtual void SetUp() {
    cache_.reset(new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("DeviceTokenFetcherTest")));
    service_.set_backend(&backend_);
    data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
    data_store_->AddObserver(&observer_);
  }

  virtual void TearDown() {
    loop_.RunAllPending();
    data_store_->RemoveObserver(&observer_);
  }

  void FetchToken(DeviceTokenFetcher* fetcher) {
    data_store_->SetupForTesting("", "fake_device_id", "fake_user_name",
                                 "fake_auth_token", true);
    fetcher->FetchToken();
  }

  MessageLoop loop_;
  MockDeviceManagementBackend backend_;
  MockDeviceManagementService service_;
  scoped_ptr<CloudPolicyCacheBase> cache_;
  scoped_ptr<CloudPolicyDataStore> data_store_;
  MockTokenAvailableObserver observer_;
  PolicyNotifier notifier_;
  ScopedTempDir temp_user_data_dir_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(DeviceTokenFetcherTest, FetchToken) {
  testing::InSequence s;
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  EXPECT_CALL(observer_, OnDeviceTokenChanged());
  EXPECT_EQ("", data_store_->device_token());
  FetchToken(&fetcher);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer_);
  std::string token = data_store_->device_token();
  EXPECT_NE("", token);

  // Calling FetchToken() again should result in a new token being fetched.
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  EXPECT_CALL(observer_, OnDeviceTokenChanged());
  FetchToken(&fetcher);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer_);
  std::string token2 = data_store_->device_token();
  EXPECT_NE("", token2);
  EXPECT_NE(token, token2);
}

TEST_F(DeviceTokenFetcherTest, RetryOnError) {
  testing::InSequence s;
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorRequestFailed)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_, new DummyWorkScheduler);
  EXPECT_CALL(observer_, OnDeviceTokenChanged());
  FetchToken(&fetcher);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_NE("", data_store_->device_token());
}

TEST_F(DeviceTokenFetcherTest, UnmanagedDevice) {
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorServiceManagementNotSupported));
  EXPECT_FALSE(cache_->is_unmanaged());
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  EXPECT_CALL(observer_, OnDeviceTokenChanged()).Times(0);
  FetchToken(&fetcher);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_EQ("", data_store_->device_token());
  EXPECT_TRUE(cache_->is_unmanaged());
}

}  // namespace policy
