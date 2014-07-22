// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/glue/local_device_info_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

const char kLocalDeviceGuid[] = "foo";
const char kSigninScopedDeviceId[] = "device_id";

class LocalDeviceInfoProviderTest : public testing::Test {
 public:
  LocalDeviceInfoProviderTest()
    : called_back_(false) {}
  virtual ~LocalDeviceInfoProviderTest() {}

  virtual void SetUp() OVERRIDE {
    provider_.reset(new LocalDeviceInfoProviderImpl());
  }

  virtual void TearDown() OVERRIDE {
    provider_.reset();
    called_back_ = false;
  }

 protected:
  void InitializeProvider() {
    // Start initialization.
    provider_->Initialize(kLocalDeviceGuid, kSigninScopedDeviceId);

    // Subscribe to the notification and wait until the callback
    // is called. The callback will quit the loop.
    base::RunLoop run_loop;
    scoped_ptr<LocalDeviceInfoProvider::Subscription> subscription(
        provider_->RegisterOnInitializedCallback(
            base::Bind(&LocalDeviceInfoProviderTest::QuitLoopOnInitialized,
                       base::Unretained(this), &run_loop)));
    run_loop.Run();
  }

  void QuitLoopOnInitialized(base::RunLoop* loop) {
    called_back_ = true;
    loop->Quit();
  }

  scoped_ptr<LocalDeviceInfoProviderImpl> provider_;

  bool called_back_;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(LocalDeviceInfoProviderTest, OnInitializedCallback) {
  ASSERT_FALSE(called_back_);

  InitializeProvider();
  EXPECT_TRUE(called_back_);
}

TEST_F(LocalDeviceInfoProviderTest, GetLocalDeviceInfo) {
  ASSERT_EQ(NULL, provider_->GetLocalDeviceInfo());

  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  EXPECT_TRUE(!!local_device_info);
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(std::string(kSigninScopedDeviceId),
            local_device_info->signin_scoped_device_id());
}

TEST_F(LocalDeviceInfoProviderTest, GetLocalSyncCacheGUID) {
  ASSERT_EQ(std::string(), provider_->GetLocalSyncCacheGUID());

  InitializeProvider();

  EXPECT_EQ(std::string(kLocalDeviceGuid), provider_->GetLocalSyncCacheGUID());
}

}  // namespace browser_sync
