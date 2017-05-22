// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/cryptauth/fake_secure_message_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptauth {
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
cryptauth::ExternalDeviceInfo CreateDeviceInfo(const std::string& suffix) {
  cryptauth::ExternalDeviceInfo device_info;
  device_info.set_friendly_device_name(std::string(kDeviceNamePrefix) + suffix);
  device_info.set_public_key(std::string(kPublicKeyPrefix) + suffix);
  device_info.set_bluetooth_address(std::string(kBluetoothAddressPrefix) +
                                   suffix);
  return device_info;
}

}  // namespace

class CryptAuthRemoteDeviceLoaderTest : public testing::Test {
 public:
  CryptAuthRemoteDeviceLoaderTest()
      : secure_message_delegate_(new cryptauth::FakeSecureMessageDelegate()),
        user_private_key_(secure_message_delegate_->GetPrivateKeyForPublicKey(
            kUserPublicKey)) {}

  ~CryptAuthRemoteDeviceLoaderTest() {}

  void OnRemoteDevicesLoaded(
      const std::vector<cryptauth::RemoteDevice>& remote_devices) {
    remote_devices_ = remote_devices;
    LoadCompleted();
  }

  MOCK_METHOD0(LoadCompleted, void());

 protected:
  // Handles deriving the PSK. Ownership will be passed to the
  // RemoteDeviceLoader under test.
  std::unique_ptr<cryptauth::FakeSecureMessageDelegate>
      secure_message_delegate_;

  // The private key of the user local device.
  std::string user_private_key_;

  // Stores the result of the RemoteDeviceLoader.
  std::vector<cryptauth::RemoteDevice> remote_devices_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthRemoteDeviceLoaderTest);
};

TEST_F(CryptAuthRemoteDeviceLoaderTest, LoadZeroDevices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  std::vector<cryptauth::RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&CryptAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(0u, remote_devices_.size());
}

TEST_F(CryptAuthRemoteDeviceLoaderTest, LoadOneDeviceWithAddress) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos(1,
                                                         CreateDeviceInfo("0"));
  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  std::vector<cryptauth::RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&CryptAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());
  EXPECT_FALSE(remote_devices_[0].persistent_symmetric_key.empty());
  EXPECT_EQ(device_infos[0].friendly_device_name(), remote_devices_[0].name);
  EXPECT_EQ(device_infos[0].public_key(), remote_devices_[0].public_key);
  EXPECT_EQ(device_infos[0].bluetooth_address(),
            remote_devices_[0].bluetooth_address);
}

TEST_F(CryptAuthRemoteDeviceLoaderTest, LoadOneDeviceWithoutAddress) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos(1,
                                                         CreateDeviceInfo("0"));
  device_infos[0].set_bluetooth_address(std::string());
  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  std::vector<cryptauth::RemoteDevice> result;
  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&CryptAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());
  EXPECT_FALSE(remote_devices_[0].persistent_symmetric_key.empty());
  EXPECT_EQ(device_infos[0].friendly_device_name(), remote_devices_[0].name);
  EXPECT_EQ(device_infos[0].public_key(), remote_devices_[0].public_key);
  EXPECT_EQ("", remote_devices_[0].bluetooth_address);
}

TEST_F(CryptAuthRemoteDeviceLoaderTest, LoadThreeRemoteDevices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  device_infos.push_back(CreateDeviceInfo("0"));
  device_infos.push_back(CreateDeviceInfo("1"));
  device_infos.push_back(CreateDeviceInfo("2"));

  // Devices 0 and 1 do not have a Bluetooth address, but device 2 does.
  device_infos[0].set_bluetooth_address(std::string());
  device_infos[1].set_bluetooth_address(std::string());

  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::Bind(&CryptAuthRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                 base::Unretained(this)));

  EXPECT_EQ(3u, remote_devices_.size());
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(remote_devices_[i].persistent_symmetric_key.empty());
    EXPECT_EQ(device_infos[i].friendly_device_name(), remote_devices_[i].name);
    EXPECT_EQ(device_infos[i].public_key(), remote_devices_[i].public_key);
    EXPECT_EQ(device_infos[i].bluetooth_address(),
              remote_devices_[i].bluetooth_address);
  }
}

}  // namespace cryptauth
