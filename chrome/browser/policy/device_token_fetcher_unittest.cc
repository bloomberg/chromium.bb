// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
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

  MOCK_METHOD0(OnTokenSuccess, void());
  MOCK_METHOD0(OnTokenError, void());
  MOCK_METHOD0(OnNotManaged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTokenAvailableObserver);
};

class DeviceTokenFetcherTest : public testing::Test {
 protected:
  DeviceTokenFetcherTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    fetcher_ = NewTestFetcher(temp_user_data_dir_.path());
    fetcher_->StartFetching();
  }

  virtual void TearDown() {
    backend_.reset();
    token_service_.reset();
    loop_.RunAllPending();
  }

  void SimulateSuccessfulLoginAndRunPending() {
    loop_.RunAllPending();
    token_service_->IssueAuthTokenForTest(
        GaiaConstants::kDeviceManagementService, kTestToken);
    loop_.RunAllPending();
  }

  DeviceTokenFetcher* NewTestFetcher(const FilePath& token_dir) {
    token_service_.reset(new TokenService);
    backend_.reset(new MockDeviceManagementBackend());
    return new DeviceTokenFetcher(
        backend_.get(),
        token_service_.get(),
        token_dir.Append(FILE_PATH_LITERAL("test-token-file.txt")));
  }

  static void GetDeviceTokenPath(const DeviceTokenFetcher* fetcher,
                                 FilePath* path) {
    fetcher->GetDeviceTokenPath(path);
  }

  const std::string& device_id(const DeviceTokenFetcher* fetcher) {
    return fetcher->device_id_;
  }

  MessageLoop loop_;
  scoped_ptr<TokenService> token_service_;
  scoped_ptr<MockDeviceManagementBackend> backend_;
  ScopedTempDir temp_user_data_dir_;
  scoped_refptr<DeviceTokenFetcher> fetcher_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(DeviceTokenFetcherTest, IsPending) {
  ASSERT_TRUE(fetcher_->IsTokenPending());
  backend_->AllShouldSucceed();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
}

TEST_F(DeviceTokenFetcherTest, StoreAndLoad) {
  backend_->AllShouldSucceed();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  std::string device_token = fetcher_->GetDeviceToken();
  std::string device_id = fetcher_->GetDeviceID();
  ASSERT_NE("", device_id);

  FilePath token_path;
  GetDeviceTokenPath(fetcher_, &token_path);
  scoped_refptr<DeviceTokenFetcher> fetcher2(
      new DeviceTokenFetcher(backend_.get(), token_service_.get(), token_path));
  fetcher2->StartFetching();
  loop_.RunAllPending();
  ASSERT_EQ(device_id, fetcher2->GetDeviceID());
  ASSERT_EQ(device_token, fetcher2->GetDeviceToken());
}

TEST_F(DeviceTokenFetcherTest, SimpleFetchSingleLogin) {
  backend_->AllShouldSucceed();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  ASSERT_TRUE(fetcher_->IsTokenValid());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);
}

TEST_F(DeviceTokenFetcherTest, SimpleFetchDoubleLogin) {
  backend_->AllShouldSucceed();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);

  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token2(fetcher_->GetDeviceToken());
  EXPECT_NE("", token2);
  EXPECT_EQ(token, token2);
}

TEST_F(DeviceTokenFetcherTest, FetchBetweenBrowserLaunchAndNotify) {
  MockTokenAvailableObserver observer;
  DeviceTokenFetcher::ObserverRegistrar registrar;
  registrar.Init(fetcher_);
  registrar.AddObserver(&observer);
  EXPECT_CALL(observer, OnTokenSuccess()).Times(1);
  EXPECT_CALL(observer, OnTokenError()).Times(0);
  backend_->AllShouldSucceed();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);
  Mock::VerifyAndClearExpectations(&observer);

  // Swap out the fetchers, including copying the device management token on
  // disk to where the new fetcher expects it.
  registrar.RemoveAll();
  fetcher_ = NewTestFetcher(temp_user_data_dir_.path());
  registrar.Init(fetcher_);
  fetcher_->StartFetching();
  ASSERT_TRUE(fetcher_->IsTokenPending());
  loop_.RunAllPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token2(fetcher_->GetDeviceToken());
  EXPECT_NE("", token2);
  EXPECT_EQ(token, token2);
}

TEST_F(DeviceTokenFetcherTest, FailedServerRequest) {
  MockTokenAvailableObserver observer;
  DeviceTokenFetcher::ObserverRegistrar registrar;
  registrar.Init(fetcher_);
  registrar.AddObserver(&observer);
  EXPECT_CALL(observer, OnTokenSuccess()).Times(0);
  EXPECT_CALL(observer, OnTokenError()).Times(1);
  backend_->AllShouldFail();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_EQ("", token);
}

TEST_F(DeviceTokenFetcherTest, UnmanagedDevice) {
  FilePath token_path;
  GetDeviceTokenPath(fetcher_, &token_path);
  file_util::WriteFile(token_path, "foo", 3);
  ASSERT_TRUE(file_util::PathExists(token_path));
  backend_->UnmanagedDevice();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  SimulateSuccessfulLoginAndRunPending();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  ASSERT_EQ("", fetcher_->GetDeviceToken());
  ASSERT_EQ("", device_id(fetcher_));
  ASSERT_FALSE(file_util::PathExists(token_path));
}

}  // namespace policy
