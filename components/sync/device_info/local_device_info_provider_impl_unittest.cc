// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/local_device_info_provider_impl.h"

#include "components/version_info/version_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

const char kLocalDeviceGuid[] = "foo";
const char kLocalDeviceSessionName[] = "bar";
const char kSigninScopedDeviceId[] = "device_id";

class LocalDeviceInfoProviderImplTest : public testing::Test {
 public:
  LocalDeviceInfoProviderImplTest() {}
  ~LocalDeviceInfoProviderImplTest() override {}

  void SetUp() override {
    provider_ = std::make_unique<LocalDeviceInfoProviderImpl>(
        version_info::Channel::UNKNOWN,
        version_info::GetVersionStringWithModifier("UNKNOWN"), false);
  }

  void TearDown() override {
    provider_.reset();
  }

 protected:
  void InitializeProvider() { InitializeProvider(kLocalDeviceGuid); }

  void InitializeProvider(const std::string& guid) {
    provider_->Initialize(guid, kLocalDeviceSessionName, kSigninScopedDeviceId);
  }

  std::unique_ptr<LocalDeviceInfoProviderImpl> provider_;
};

TEST_F(LocalDeviceInfoProviderImplTest, GetLocalDeviceInfo) {
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());
  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  ASSERT_NE(nullptr, local_device_info);
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(std::string(kSigninScopedDeviceId),
            local_device_info->signin_scoped_device_id());
  EXPECT_EQ(kLocalDeviceSessionName, local_device_info->client_name());

  EXPECT_EQ(provider_->GetSyncUserAgent(),
            local_device_info->sync_user_agent());

  provider_->Clear();
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());
}

TEST_F(LocalDeviceInfoProviderImplTest, GetLocalSyncCacheGUID) {
  EXPECT_TRUE(provider_->GetLocalSyncCacheGUID().empty());

  InitializeProvider();
  EXPECT_EQ(std::string(kLocalDeviceGuid), provider_->GetLocalSyncCacheGUID());

  provider_->Clear();
  EXPECT_TRUE(provider_->GetLocalSyncCacheGUID().empty());
}

}  // namespace syncer
