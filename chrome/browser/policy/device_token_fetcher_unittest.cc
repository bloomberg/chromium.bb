// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/mock_cloud_policy_data_store.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

const char kTestToken[] = "device_token_fetcher_test_auth_token";

using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::Mock;

class DeviceTokenFetcherTest : public testing::Test {
 protected:
  DeviceTokenFetcherTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
  }

  virtual void SetUp() {
    cache_.reset(new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("DeviceTokenFetcherTest"),
        false  /* wait_for_policy_fetch */));
    EXPECT_CALL(service_, CreateBackend())
        .Times(AnyNumber())
        .WillRepeatedly(MockDeviceManagementServiceProxyBackend(&backend_));
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

  void CreateNewWaitingCache() {
    cache_.reset(new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("DeviceTokenFetcherTest"),
        true  /* wait_for_policy_fetch */));
    // Make this cache's disk cache ready, but have it still waiting for a
    // policy fetch.
    cache_->Load();
    loop_.RunAllPending();
    ASSERT_TRUE(cache_->last_policy_refresh_time().is_null());
    ASSERT_FALSE(cache_->IsReady());
  }

  MessageLoop loop_;
  MockDeviceManagementBackend backend_;
  MockDeviceManagementService service_;
  scoped_ptr<CloudPolicyCacheBase> cache_;
  scoped_ptr<CloudPolicyDataStore> data_store_;
  MockCloudPolicyDataStoreObserver observer_;
  PolicyNotifier notifier_;
  ScopedTempDir temp_user_data_dir_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(DeviceTokenFetcherTest, FetchToken) {
  testing::InSequence s;
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
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
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
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
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
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
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
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

TEST_F(DeviceTokenFetcherTest, DontSetFetchingDone) {
  CreateNewWaitingCache();
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  EXPECT_FALSE(cache_->IsReady());
}

TEST_F(DeviceTokenFetcherTest, DontSetFetchingDoneWithoutPolicyFetch) {
  CreateNewWaitingCache();
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  EXPECT_CALL(observer_, OnDeviceTokenChanged());
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  FetchToken(&fetcher);
  loop_.RunAllPending();
  // On successful token fetching the cache isn't set to ready, since the next
  // step is to fetch policy. Only failures to fetch the token should make
  // the cache ready.
  EXPECT_FALSE(cache_->IsReady());
}

TEST_F(DeviceTokenFetcherTest, SetFetchingDoneWhenUnmanaged) {
  CreateNewWaitingCache();
  cache_->SetUnmanaged();
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  EXPECT_TRUE(cache_->IsReady());
}

TEST_F(DeviceTokenFetcherTest, SetFetchingDoneOnFailures) {
  CreateNewWaitingCache();
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorRequestFailed));
  DeviceTokenFetcher fetcher(&service_, cache_.get(), data_store_.get(),
                             &notifier_);
  FetchToken(&fetcher);
  loop_.RunAllPending();
  // This is the opposite case of DontSetFetchingDone1.
  EXPECT_TRUE(cache_->IsReady());
}

}  // namespace policy
