// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/cloud_policy_cache.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

const char kTestToken[] = "device_token_fetcher_test_auth_token";

using testing::_;
using testing::Mock;

class MockTokenAvailableObserver : public DeviceTokenFetcher::Observer {
 public:
  MockTokenAvailableObserver() {}
  virtual ~MockTokenAvailableObserver() {}

  MOCK_METHOD0(OnDeviceTokenAvailable, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTokenAvailableObserver);
};

class MockDeviceManagementService : public DeviceManagementService {
 public:
  MockDeviceManagementService()
      : DeviceManagementService(""),
        backend_(NULL) {}
  virtual ~MockDeviceManagementService() {}

  void set_backend(DeviceManagementBackend* backend) {
    backend_ = backend;
  }
  virtual DeviceManagementBackend* CreateBackend();

 private:
  DeviceManagementBackend* backend_;  // weak
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementService);
};

// This proxy class is used so that expectations can be defined for a single
// persistant instance of DMBackend while the DeviceTokenFetcher under test
// merrily creates and destroys proxies.
class ProxyDeviceManagementBackend : public DeviceManagementBackend {
 public:
  explicit ProxyDeviceManagementBackend(DeviceManagementBackend* backend)
      : backend_(backend) {
  }
  virtual ~ProxyDeviceManagementBackend() {}

  virtual void ProcessRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate) {
    backend_->ProcessRegisterRequest(auth_token, device_id, request, delegate);
  }
  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate) {
    backend_->ProcessUnregisterRequest(device_management_token, device_id,
                                       request, delegate);
  }
  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate) {
    backend_->ProcessPolicyRequest(device_management_token, device_id,
                                   request, delegate);
  }
  virtual void ProcessCloudPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::CloudPolicyRequest& request,
      DevicePolicyResponseDelegate* delegate) {
    backend_->ProcessCloudPolicyRequest(device_management_token, device_id,
                                        request, delegate);
  }

 private:
  DeviceManagementBackend* backend_;  // weak
  DISALLOW_COPY_AND_ASSIGN(ProxyDeviceManagementBackend);
};

DeviceManagementBackend*
    MockDeviceManagementService::CreateBackend()  {
  return new ProxyDeviceManagementBackend(backend_);
}

class DeviceTokenFetcherTest : public testing::Test {
 protected:
  DeviceTokenFetcherTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
  }

  virtual void SetUp() {
    cache_.reset(new CloudPolicyCache(
        temp_user_data_dir_.path().AppendASCII("DeviceTokenFetcherTest")));
    service_.set_backend(&backend_);
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  MessageLoop loop_;
  MockDeviceManagementBackend backend_;
  MockDeviceManagementService service_;
  scoped_ptr<CloudPolicyCache> cache_;
  ScopedTempDir temp_user_data_dir_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(DeviceTokenFetcherTest, FetchToken) {
  testing::InSequence s;
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  DeviceTokenFetcher fetcher(&service_, cache_.get());
  MockTokenAvailableObserver observer;
  EXPECT_CALL(observer, OnDeviceTokenAvailable());
  fetcher.AddObserver(&observer);
  EXPECT_EQ("", fetcher.GetDeviceToken());
  fetcher.FetchToken("fake_auth_token", "fake_device_id");
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  std::string token = fetcher.GetDeviceToken();
  EXPECT_NE("", token);

  // Calling FetchToken() again should result in a new token being fetched.
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  EXPECT_CALL(observer, OnDeviceTokenAvailable());
  fetcher.FetchToken("fake_auth_token", "fake_device_id");
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  std::string token2 = fetcher.GetDeviceToken();
  EXPECT_NE("", token2);
  EXPECT_NE(token, token2);
  fetcher.RemoveObserver(&observer);
}

TEST_F(DeviceTokenFetcherTest, RetryOnError) {
  testing::InSequence s;
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorRequestFailed)).WillOnce(
      MockDeviceManagementBackendSucceedRegister());
  DeviceTokenFetcher fetcher(&service_, cache_.get(), 0, 0);
  MockTokenAvailableObserver observer;
  EXPECT_CALL(observer, OnDeviceTokenAvailable());
  fetcher.AddObserver(&observer);
  fetcher.FetchToken("fake_auth_token", "fake_device_id");
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_NE("", fetcher.GetDeviceToken());
  fetcher.RemoveObserver(&observer);
}

TEST_F(DeviceTokenFetcherTest, UnmanagedDevice) {
  EXPECT_CALL(backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorServiceManagementNotSupported));
  EXPECT_FALSE(cache_->is_unmanaged());
  DeviceTokenFetcher fetcher(&service_, cache_.get());
  MockTokenAvailableObserver observer;
  EXPECT_CALL(observer, OnDeviceTokenAvailable()).Times(0);
  fetcher.AddObserver(&observer);
  fetcher.FetchToken("fake_auth_token", "fake_device_id");
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ("", fetcher.GetDeviceToken());
  EXPECT_TRUE(cache_->is_unmanaged());
  fetcher.RemoveObserver(&observer);
}

}  // namespace policy
