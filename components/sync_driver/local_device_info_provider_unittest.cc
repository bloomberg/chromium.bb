// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync_driver/local_device_info_provider_impl.h"
#include "components/version_info/version_info.h"
#include "sync/util/get_session_name.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_driver::DeviceInfo;
using sync_driver::LocalDeviceInfoProvider;

namespace browser_sync {

const char kLocalDeviceGuid[] = "foo";
const char kSigninScopedDeviceId[] = "device_id";

class SyncLocalDeviceInfoProviderTest : public testing::Test {
 public:
  SyncLocalDeviceInfoProviderTest() : called_back_(false) {}
  ~SyncLocalDeviceInfoProviderTest() override {}

  void SetUp() override {
    provider_.reset(new LocalDeviceInfoProviderImpl(
        version_info::Channel::UNKNOWN,
        version_info::GetVersionStringWithModifier("UNKNOWN"), false));
  }

  void TearDown() override {
    provider_.reset();
    called_back_ = false;
  }

 protected:
  void InitializeProvider() {
    // Start initialization.
    provider_->Initialize(kLocalDeviceGuid,
                          kSigninScopedDeviceId,
                          message_loop_.task_runner());

    // Subscribe to the notification and wait until the callback
    // is called. The callback will quit the loop.
    base::RunLoop run_loop;
    std::unique_ptr<LocalDeviceInfoProvider::Subscription> subscription(
        provider_->RegisterOnInitializedCallback(
            base::Bind(&SyncLocalDeviceInfoProviderTest::QuitLoopOnInitialized,
                       base::Unretained(this), &run_loop)));
    run_loop.Run();
  }

  void QuitLoopOnInitialized(base::RunLoop* loop) {
    called_back_ = true;
    loop->Quit();
  }

  std::unique_ptr<LocalDeviceInfoProviderImpl> provider_;

  bool called_back_;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(SyncLocalDeviceInfoProviderTest, OnInitializedCallback) {
  ASSERT_FALSE(called_back_);

  InitializeProvider();
  EXPECT_TRUE(called_back_);
}

TEST_F(SyncLocalDeviceInfoProviderTest, GetLocalDeviceInfo) {
  ASSERT_EQ(NULL, provider_->GetLocalDeviceInfo());

  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  EXPECT_TRUE(local_device_info);
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(std::string(kSigninScopedDeviceId),
            local_device_info->signin_scoped_device_id());
  EXPECT_EQ(syncer::GetSessionNameSynchronouslyForTesting(),
            local_device_info->client_name());

  EXPECT_EQ(provider_->GetSyncUserAgent(),
            local_device_info->sync_user_agent());
}

TEST_F(SyncLocalDeviceInfoProviderTest, GetLocalSyncCacheGUID) {
  ASSERT_EQ(std::string(), provider_->GetLocalSyncCacheGUID());

  InitializeProvider();

  EXPECT_EQ(std::string(kLocalDeviceGuid), provider_->GetLocalSyncCacheGUID());
}

}  // namespace browser_sync
