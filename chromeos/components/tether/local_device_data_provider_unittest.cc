// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/local_device_data_provider.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {
const std::string kDefaultPublicKey = "publicKey";

const std::string kBeaconSeed1Data = "beaconSeed1Data";
const int64_t kBeaconSeed1StartMs = 1000L;
const int64_t kBeaconSeed1EndMs = 2000L;

const std::string kBeaconSeed2Data = "beaconSeed2Data";
const int64_t kBeaconSeed2StartMs = 2000L;
const int64_t kBeaconSeed2EndMs = 3000L;

cryptauth::BeaconSeed CreateBeaconSeed(const std::string& data,
                                       int64_t start_ms,
                                       int64_t end_ms) {
  cryptauth::BeaconSeed seed;
  seed.set_data(data);
  seed.set_start_time_millis(start_ms);
  seed.set_end_time_millis(end_ms);
  return seed;
}
}  // namespace

class LocalDeviceDataProviderTest : public testing::Test {
 protected:
  class MockProviderDelegate
      : public LocalDeviceDataProvider::LocalDeviceDataProviderDelegate {
   public:
    MockProviderDelegate() {}
    ~MockProviderDelegate() override {}

    MOCK_CONST_METHOD0(GetUserPublicKey, std::string());
    MOCK_CONST_METHOD0(GetSyncedDevices,
                       std::vector<cryptauth::ExternalDeviceInfo>());
  };

  LocalDeviceDataProviderTest() {
    fake_beacon_seeds_.push_back(CreateBeaconSeed(
        kBeaconSeed1Data, kBeaconSeed1StartMs, kBeaconSeed1EndMs));
    fake_beacon_seeds_.push_back(CreateBeaconSeed(
        kBeaconSeed2Data, kBeaconSeed2StartMs, kBeaconSeed2EndMs));

    // Has no public key and no BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device1;
    fake_synced_devices_.push_back(synced_device1);

    // Has no public key and some BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device2;
    synced_device2.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed1Data, kBeaconSeed1StartMs, kBeaconSeed1EndMs));
    synced_device2.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed2Data, kBeaconSeed2StartMs, kBeaconSeed2EndMs));
    fake_synced_devices_.push_back(synced_device2);

    // Has another different public key and no BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device3;
    synced_device3.set_public_key("anotherPublicKey");
    fake_synced_devices_.push_back(synced_device3);

    // Has different public key and BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device4;
    synced_device4.set_public_key("otherPublicKey");
    synced_device4.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed1Data, kBeaconSeed1StartMs, kBeaconSeed1EndMs));
    synced_device4.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed2Data, kBeaconSeed2StartMs, kBeaconSeed2EndMs));
    fake_synced_devices_.push_back(synced_device4);

    // Has public key but no BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device5;
    synced_device5.set_public_key(kDefaultPublicKey);
    fake_synced_devices_.push_back(synced_device5);

    // Has public key and BeaconSeeds.
    cryptauth::ExternalDeviceInfo synced_device6;
    synced_device6.set_public_key(kDefaultPublicKey);
    synced_device6.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed1Data, kBeaconSeed1StartMs, kBeaconSeed1EndMs));
    synced_device6.add_beacon_seeds()->CopyFrom(CreateBeaconSeed(
        kBeaconSeed2Data, kBeaconSeed2StartMs, kBeaconSeed2EndMs));
    fake_synced_devices_.push_back(synced_device6);
  }

  void SetUp() override {
    mock_delegate_ = new NiceMock<MockProviderDelegate>();

    provider_ = base::WrapUnique(
        new LocalDeviceDataProvider(base::WrapUnique(mock_delegate_)));
  }

  std::unique_ptr<LocalDeviceDataProvider> provider_;
  NiceMock<MockProviderDelegate>* mock_delegate_;

  std::vector<cryptauth::BeaconSeed> fake_beacon_seeds_;
  std::vector<cryptauth::ExternalDeviceInfo> fake_synced_devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalDeviceDataProviderTest);
};

TEST_F(LocalDeviceDataProviderTest, TestGetLocalDeviceData_NoPublicKey) {
  ON_CALL(*mock_delegate_, GetUserPublicKey())
      .WillByDefault(Return(std::string()));
  ON_CALL(*mock_delegate_, GetSyncedDevices())
      .WillByDefault(Return(fake_synced_devices_));

  std::string public_key;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  EXPECT_FALSE(provider_->GetLocalDeviceData(&public_key, &beacon_seeds));
}

TEST_F(LocalDeviceDataProviderTest, TestGetLocalDeviceData_NoSyncedDevices) {
  ON_CALL(*mock_delegate_, GetUserPublicKey())
      .WillByDefault(Return(kDefaultPublicKey));
  ON_CALL(*mock_delegate_, GetSyncedDevices())
      .WillByDefault(Return(std::vector<cryptauth::ExternalDeviceInfo>()));

  std::string public_key;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  EXPECT_FALSE(provider_->GetLocalDeviceData(&public_key, &beacon_seeds));
}

TEST_F(LocalDeviceDataProviderTest,
       TestGetLocalDeviceData_NoSyncedDeviceMatchingPublicKey) {
  ON_CALL(*mock_delegate_, GetUserPublicKey())
      .WillByDefault(Return(kDefaultPublicKey));
  ON_CALL(*mock_delegate_, GetSyncedDevices())
      .WillByDefault(Return(std::vector<cryptauth::ExternalDeviceInfo>{
          fake_synced_devices_[0], fake_synced_devices_[1],
          fake_synced_devices_[2], fake_synced_devices_[3]}));

  std::string public_key;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  EXPECT_FALSE(provider_->GetLocalDeviceData(&public_key, &beacon_seeds));
}

TEST_F(LocalDeviceDataProviderTest,
       TestGetLocalDeviceData_SyncedDeviceIncludesPublicKeyButNoBeaconSeeds) {
  ON_CALL(*mock_delegate_, GetUserPublicKey())
      .WillByDefault(Return(kDefaultPublicKey));
  ON_CALL(*mock_delegate_, GetSyncedDevices())
      .WillByDefault(Return(std::vector<cryptauth::ExternalDeviceInfo>{
          fake_synced_devices_[4],
      }));

  std::string public_key;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  EXPECT_FALSE(provider_->GetLocalDeviceData(&public_key, &beacon_seeds));
}

TEST_F(LocalDeviceDataProviderTest, TestGetLocalDeviceData_Success) {
  ON_CALL(*mock_delegate_, GetUserPublicKey())
      .WillByDefault(Return(kDefaultPublicKey));
  ON_CALL(*mock_delegate_, GetSyncedDevices())
      .WillByDefault(Return(fake_synced_devices_));

  std::string public_key;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  EXPECT_TRUE(provider_->GetLocalDeviceData(&public_key, &beacon_seeds));

  EXPECT_EQ(kDefaultPublicKey, public_key);

  ASSERT_EQ(fake_beacon_seeds_.size(), beacon_seeds.size());
  for (size_t i = 0; i < fake_beacon_seeds_.size(); i++) {
    // Note: google::protobuf::util::MessageDifferencer can only be used to diff
    // Message, but BeaconSeed derives from the incompatible MessageLite class.
    cryptauth::BeaconSeed expected = fake_beacon_seeds_[i];
    cryptauth::BeaconSeed actual = beacon_seeds[i];
    EXPECT_EQ(expected.data(), actual.data());
    EXPECT_EQ(expected.start_time_millis(), actual.start_time_millis());
    EXPECT_EQ(expected.end_time_millis(), actual.end_time_millis());
  }
}

}  // namespace tether

}  // namespace chromeos
