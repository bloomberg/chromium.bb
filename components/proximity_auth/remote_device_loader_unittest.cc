// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_device_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/proximity_auth/cryptauth/fake_secure_message_delegate.h"
#include "components/proximity_auth/proximity_auth_pref_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

// Prefixes for RemoteDevice fields.
const char kDeviceNamePrefix[] = "device";
const char kPublicKeyPrefix[] = "pk";
const char kBluetoothAddressPrefix[] = "11:22:33:44:55:0";

// The id of the user who the remote devices belong to.
const char kUserId[] = "example@gmail.com";

// The public key of the user's local device.
const char kUserPublicKey[] = "User public key";

// Creates and returns an ExternalDeviceInfo proto with the fields appended with
// |suffix|.
cryptauth::ExternalDeviceInfo CreateUnlockKey(const std::string& suffix) {
  cryptauth::ExternalDeviceInfo unlock_key;
  unlock_key.set_friendly_device_name(std::string(kDeviceNamePrefix) + suffix);
  unlock_key.set_public_key(std::string(kPublicKeyPrefix) + suffix);
  unlock_key.set_bluetooth_address(std::string(kBluetoothAddressPrefix) +
                                   suffix);
  return unlock_key;
}

class MockProximityAuthPrefManager : public ProximityAuthPrefManager {
 public:
  MockProximityAuthPrefManager() : ProximityAuthPrefManager(nullptr) {}
  ~MockProximityAuthPrefManager() override {}

  MOCK_CONST_METHOD1(GetDeviceAddress, std::string(const std::string&));
};

}  // namespace

class ProximityAuthRemoteDeviceLoaderTest : public testing::Test {
 public:
  ProximityAuthRemoteDeviceLoaderTest()
      : secure_message_delegate_(new FakeSecureMessageDelegate()),
        user_private_key_(secure_message_delegate_->GetPrivateKeyForPublicKey(
            kUserPublicKey)),
        pref_manager_(new MockProximityAuthPrefManager()) {}

  ~ProximityAuthRemoteDeviceLoaderTest() {}

  void OnRemoteDevicesLoaded(const std::vector<RemoteDevice>& remote_devices) {
    remote_devices_ = remote_devices;
    LoadCompleted();
  }

  MOCK_METHOD0(LoadCompleted, void());

 protected:
  // Handles deriving the PSK. Ownership will be passed to the
  // RemoteDeviceLoader under test.
  std::unique_ptr<FakeSecureMessageDelegate> secure_message_delegate_;

  // The private key of the user local device.
  std::string user_private_key_;

  // Stores the result of the RemoteDeviceLoader.
  std::vector<RemoteDevice> remote_devices_;

  // Stores the bluetooth address for BLE devices.
  std::unique_ptr<MockProximityAuthPrefManager> pref_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthRemoteDeviceLoaderTest);
};

TEST_F(ProximityAuthRemoteDeviceLoaderTest, LoadZeroDevices) {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys;
  RemoteDeviceLoader loader(unlock_keys, user_private_key_, kUserId,
                            std::move(secure_message_delegate_),
                            pref_manager_.get());

  std::vector<RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&ProximityAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(0u, remote_devices_.size());
}

TEST_F(ProximityAuthRemoteDeviceLoaderTest, LoadOneClassicRemoteDevice) {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys(1,
                                                         CreateUnlockKey("1"));
  RemoteDeviceLoader loader(unlock_keys, user_private_key_, kUserId,
                            std::move(secure_message_delegate_),
                            pref_manager_.get());

  std::vector<RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&ProximityAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());
  EXPECT_FALSE(remote_devices_[0].persistent_symmetric_key.empty());
  EXPECT_EQ(unlock_keys[0].friendly_device_name(), remote_devices_[0].name);
  EXPECT_EQ(unlock_keys[0].public_key(), remote_devices_[0].public_key);
  EXPECT_EQ(unlock_keys[0].bluetooth_address(),
            remote_devices_[0].bluetooth_address);
  EXPECT_EQ(RemoteDevice::BLUETOOTH_CLASSIC, remote_devices_[0].bluetooth_type);
}

TEST_F(ProximityAuthRemoteDeviceLoaderTest, LoadOneBLERemoteDevice) {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys(1,
                                                         CreateUnlockKey("1"));
  unlock_keys[0].set_bluetooth_address(std::string());
  RemoteDeviceLoader loader(unlock_keys, user_private_key_, kUserId,
                            std::move(secure_message_delegate_),
                            pref_manager_.get());

  std::string ble_address = "00:00:00:00:00:00";
  EXPECT_CALL(*pref_manager_, GetDeviceAddress(testing::_))
      .WillOnce(testing::Return(ble_address));

  std::vector<RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&ProximityAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());
  EXPECT_FALSE(remote_devices_[0].persistent_symmetric_key.empty());
  EXPECT_EQ(unlock_keys[0].friendly_device_name(), remote_devices_[0].name);
  EXPECT_EQ(unlock_keys[0].public_key(), remote_devices_[0].public_key);
  EXPECT_EQ(ble_address, remote_devices_[0].bluetooth_address);
  EXPECT_EQ(RemoteDevice::BLUETOOTH_LE, remote_devices_[0].bluetooth_type);
}

TEST_F(ProximityAuthRemoteDeviceLoaderTest, LoadThreeRemoteDevice) {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys;
  unlock_keys.push_back(CreateUnlockKey("1"));
  unlock_keys.push_back(CreateUnlockKey("2"));
  unlock_keys.push_back(CreateUnlockKey("3"));
  RemoteDeviceLoader loader(unlock_keys, user_private_key_, kUserId,
                            std::move(secure_message_delegate_),
                            pref_manager_.get());

  std::vector<RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&ProximityAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(3u, remote_devices_.size());
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(remote_devices_[i].persistent_symmetric_key.empty());
    EXPECT_EQ(unlock_keys[i].friendly_device_name(), remote_devices_[i].name);
    EXPECT_EQ(unlock_keys[i].public_key(), remote_devices_[i].public_key);
    EXPECT_EQ(unlock_keys[i].bluetooth_address(),
              remote_devices_[i].bluetooth_address);
  }
}

}  // namespace proximity_auth
