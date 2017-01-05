// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_beacon_seed_fetcher.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::Return;

namespace cryptauth {

namespace {

const std::string fake_beacon_seed1_data = "fakeBeaconSeed1Data";
const int64_t fake_beacon_seed1_start_ms = 1000L;
const int64_t fake_beacon_seed1_end_ms = 2000L;

const std::string fake_beacon_seed2_data = "fakeBeaconSeed2Data";
const int64_t fake_beacon_seed2_start_ms = 2000L;
const int64_t fake_beacon_seed2_end_ms = 3000L;

const std::string fake_beacon_seed3_data = "fakeBeaconSeed3Data";
const int64_t fake_beacon_seed3_start_ms = 1000L;
const int64_t fake_beacon_seed3_end_ms = 2000L;

const std::string fake_beacon_seed4_data = "fakeBeaconSeed4Data";
const int64_t fake_beacon_seed4_start_ms = 2000L;
const int64_t fake_beacon_seed4_end_ms = 3000L;

const std::string public_key1 = "publicKey1";
const std::string public_key2 = "publicKey2";

class MockDeviceManager : public CryptAuthDeviceManager {
 public:
  MockDeviceManager() {}
  ~MockDeviceManager() override {}

  MOCK_CONST_METHOD0(GetSyncedDevices, std::vector<ExternalDeviceInfo>());
};

RemoteDevice CreateRemoteDevice(const std::string& public_key) {
  RemoteDevice remote_device;
  remote_device.public_key = public_key;
  return remote_device;
}

ExternalDeviceInfo CreateFakeInfo1() {
  BeaconSeed seed1;
  seed1.set_data(fake_beacon_seed1_data);
  seed1.set_start_time_millis(fake_beacon_seed1_start_ms);
  seed1.set_end_time_millis(fake_beacon_seed1_end_ms);

  BeaconSeed seed2;
  seed2.set_data(fake_beacon_seed2_data);
  seed2.set_start_time_millis(fake_beacon_seed2_start_ms);
  seed2.set_end_time_millis(fake_beacon_seed2_end_ms);

  ExternalDeviceInfo info1;
  info1.set_public_key(public_key1);
  info1.add_beacon_seeds()->CopyFrom(seed1);
  info1.add_beacon_seeds()->CopyFrom(seed2);
  return info1;
}

ExternalDeviceInfo CreateFakeInfo2() {
  BeaconSeed seed3;
  seed3.set_data(fake_beacon_seed3_data);
  seed3.set_start_time_millis(fake_beacon_seed3_start_ms);
  seed3.set_end_time_millis(fake_beacon_seed3_end_ms);

  BeaconSeed seed4;
  seed4.set_data(fake_beacon_seed4_data);
  seed4.set_start_time_millis(fake_beacon_seed4_start_ms);
  seed4.set_end_time_millis(fake_beacon_seed4_end_ms);

  ExternalDeviceInfo info2;
  info2.set_public_key(public_key2);
  info2.add_beacon_seeds()->CopyFrom(seed3);
  info2.add_beacon_seeds()->CopyFrom(seed4);
  return info2;
}

}  // namespace

class CryptAuthRemoteBeaconSeedFetcherTest : public testing::Test {
 protected:
  CryptAuthRemoteBeaconSeedFetcherTest()
      : fake_info1_(CreateFakeInfo1()), fake_info2_(CreateFakeInfo2()) {}

  void SetUp() override {
    mock_device_manager_ = base::MakeUnique<MockDeviceManager>();
    fetcher_ = base::MakeUnique<StrictMock<RemoteBeaconSeedFetcher>>(
        mock_device_manager_.get());
  }

  std::unique_ptr<RemoteBeaconSeedFetcher> fetcher_;
  std::unique_ptr<MockDeviceManager> mock_device_manager_;

  const ExternalDeviceInfo fake_info1_;
  const ExternalDeviceInfo fake_info2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthRemoteBeaconSeedFetcherTest);
};

TEST_F(CryptAuthRemoteBeaconSeedFetcherTest, TestRemoteDeviceWithNoPublicKey) {
  RemoteDevice device = CreateRemoteDevice("");

  std::vector<BeaconSeed> seeds;
  EXPECT_FALSE(fetcher_->FetchSeedsForDevice(device, &seeds));
}

TEST_F(CryptAuthRemoteBeaconSeedFetcherTest, TestNoSyncedDevices) {
  RemoteDevice device = CreateRemoteDevice(public_key1);

  EXPECT_CALL(*mock_device_manager_, GetSyncedDevices())
      .WillOnce(Return(std::vector<ExternalDeviceInfo>()));

  std::vector<BeaconSeed> seeds;
  EXPECT_FALSE(fetcher_->FetchSeedsForDevice(device, &seeds));
}

TEST_F(CryptAuthRemoteBeaconSeedFetcherTest, TestDeviceHasDifferentPublicKey) {
  // A public key which is different from the public keys of all of the synced
  // devices.
  RemoteDevice device = CreateRemoteDevice("differentPublicKey");

  std::vector<ExternalDeviceInfo> device_infos = {fake_info1_, fake_info2_};
  EXPECT_CALL(*mock_device_manager_, GetSyncedDevices())
      .WillOnce(Return(device_infos));

  std::vector<BeaconSeed> seeds;
  EXPECT_FALSE(fetcher_->FetchSeedsForDevice(device, &seeds));
}

TEST_F(CryptAuthRemoteBeaconSeedFetcherTest, TestSuccess) {
  RemoteDevice device1 = CreateRemoteDevice(public_key1);
  RemoteDevice device2 = CreateRemoteDevice(public_key2);

  std::vector<ExternalDeviceInfo> device_infos = {fake_info1_, fake_info2_};
  EXPECT_CALL(*mock_device_manager_, GetSyncedDevices())
      .Times(2)
      .WillRepeatedly(Return(device_infos));

  std::vector<BeaconSeed> seeds1;
  ASSERT_TRUE(fetcher_->FetchSeedsForDevice(device1, &seeds1));
  ASSERT_EQ(static_cast<size_t>(2), seeds1.size());
  EXPECT_EQ(fake_beacon_seed1_data, seeds1[0].data());
  EXPECT_EQ(fake_beacon_seed1_start_ms, seeds1[0].start_time_millis());
  EXPECT_EQ(fake_beacon_seed1_end_ms, seeds1[0].end_time_millis());
  EXPECT_EQ(fake_beacon_seed2_data, seeds1[1].data());
  EXPECT_EQ(fake_beacon_seed2_start_ms, seeds1[1].start_time_millis());
  EXPECT_EQ(fake_beacon_seed2_end_ms, seeds1[1].end_time_millis());

  std::vector<BeaconSeed> seeds2;
  ASSERT_TRUE(fetcher_->FetchSeedsForDevice(device2, &seeds2));
  ASSERT_EQ(static_cast<size_t>(2), seeds2.size());
  EXPECT_EQ(fake_beacon_seed3_data, seeds2[0].data());
  EXPECT_EQ(fake_beacon_seed3_start_ms, seeds2[0].start_time_millis());
  EXPECT_EQ(fake_beacon_seed3_end_ms, seeds2[0].end_time_millis());
  EXPECT_EQ(fake_beacon_seed4_data, seeds2[1].data());
  EXPECT_EQ(fake_beacon_seed4_start_ms, seeds2[1].start_time_millis());
  EXPECT_EQ(fake_beacon_seed4_end_ms, seeds2[1].end_time_millis());
}

}  // namespace cryptauth
