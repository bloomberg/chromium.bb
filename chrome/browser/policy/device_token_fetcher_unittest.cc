// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using testing::_;
using testing::Mock;

class MockDeviceTokenPathProvider
    : public StoredDeviceTokenPathProvider {
 public:
  MockDeviceTokenPathProvider() {
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
  }

  virtual ~MockDeviceTokenPathProvider() {}

  virtual bool GetPath(FilePath* path) const {
    if (!file_util::PathExists(temp_user_data_dir_.path()))
      return false;
    *path = temp_user_data_dir_.path().Append(
        FILE_PATH_LITERAL("temp_token_file"));
    return true;
  }

 private:
  ScopedTempDir temp_user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceTokenPathProvider);
};

class MockTokenAvailableObserver : public NotificationObserver {
 public:
  MockTokenAvailableObserver() {}
  virtual ~MockTokenAvailableObserver() {}

  MOCK_METHOD3(Observe, void(
      NotificationType type,
      const NotificationSource& source,
      const NotificationDetails& details));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTokenAvailableObserver);
};

class DeviceTokenFetcherTest : public testing::Test {
 protected:
  DeviceTokenFetcherTest() {
    backend_ = new MockDeviceManagementBackend();
    path_provider_ = new MockDeviceTokenPathProvider();
    fetcher_.reset(new DeviceTokenFetcher(backend_, path_provider_));
    fetcher_->StartFetching();
  }

  virtual void SetUp() {
    ui_thread_.reset(new BrowserThread(BrowserThread::UI, &loop_));
    file_thread_.reset(new BrowserThread(BrowserThread::FILE, &loop_));
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  void SimulateSuccessfulLogin() {
    const std::string service(GaiaConstants::kDeviceManagementService);
    const std::string auth_token(kFakeGaiaAuthToken);
    const Source<TokenService> source(NULL);
    TokenService::TokenAvailableDetails details(service, auth_token);
    NotificationService::current()->Notify(
        NotificationType::TOKEN_AVAILABLE,
        source,
        Details<const TokenService::TokenAvailableDetails>(&details));
    loop_.RunAllPending();
  }

  MockDeviceManagementBackend* backend_;  // weak
  MockDeviceTokenPathProvider* path_provider_;  // weak
  scoped_ptr<DeviceTokenFetcher> fetcher_;

 private:
  MessageLoop loop_;
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> file_thread_;

  static const char kFakeGaiaAuthToken[];
};

const char DeviceTokenFetcherTest::kFakeGaiaAuthToken[] = "0123456789abcdef";

TEST_F(DeviceTokenFetcherTest, IsPending) {
  ASSERT_TRUE(fetcher_->IsTokenPending());
  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
}

TEST_F(DeviceTokenFetcherTest, SimpleFetchSingleLogin) {
  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  ASSERT_TRUE(fetcher_->IsTokenValid());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);
}

TEST_F(DeviceTokenFetcherTest, SimpleFetchDoubleLogin) {
  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);

  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token2(fetcher_->GetDeviceToken());
  EXPECT_NE("", token2);
  EXPECT_EQ(token, token2);
}

TEST_F(DeviceTokenFetcherTest, FetchBetweenBrowserLaunchAndNotify) {
  NotificationRegistrar registrar;
  MockTokenAvailableObserver observer;
  registrar.Add(&observer,
                NotificationType::DEVICE_TOKEN_AVAILABLE,
                NotificationService::AllSources());
  EXPECT_CALL(observer, Observe(_, _, _)).Times(1);

  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_NE("", token);

  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_CALL(observer, Observe(_, _, _)).Times(1);

  // Swap out the fetchers, including copying the device management token on
  // disk to where the new fetcher expects it.
  backend_ = new MockDeviceManagementBackend();
  FilePath old_path;
  ASSERT_TRUE(path_provider_->GetPath(&old_path));
  MockDeviceTokenPathProvider* new_provider =
      new MockDeviceTokenPathProvider();
  FilePath new_path;
  ASSERT_TRUE(new_provider->GetPath(&new_path));
  ASSERT_TRUE(file_util::Move(old_path, new_path));
  path_provider_ = new_provider;
  fetcher_.reset(new DeviceTokenFetcher(backend_, path_provider_));

  fetcher_->StartFetching();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token2(fetcher_->GetDeviceToken());
  EXPECT_NE("", token2);
  EXPECT_EQ(token, token2);
}

TEST_F(DeviceTokenFetcherTest, FailedServerRequest) {
  backend_->SetFailure(true);
  SimulateSuccessfulLogin();
  ASSERT_FALSE(fetcher_->IsTokenPending());
  const std::string token(fetcher_->GetDeviceToken());
  EXPECT_EQ("", token);
}

}  // namespace policy
