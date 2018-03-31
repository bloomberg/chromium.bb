// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_

#include <vector>

#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockRemoteCharacteristic : public RemoteCharacteristic {
 public:
  MockRemoteCharacteristic();

  MOCK_METHOD0(GetDescriptors, std::vector<scoped_refptr<RemoteDescriptor>>());
  MOCK_METHOD1(
      GetDescriptorByUuid,
      scoped_refptr<RemoteDescriptor>(const bluetooth_v2_shlib::Uuid& uuid));
  void SetRegisterNotification(bool enable, StatusCallback cb) override {}
  void SetNotification(bool enable, StatusCallback cb) override {}
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override {}
  void Read(ReadCallback callback) override {}
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 bluetooth_v2_shlib::Gatt::WriteType write_type,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override {}
  void Write(bluetooth_v2_shlib::Gatt::WriteType write_type,
             const std::vector<uint8_t>& value,
             StatusCallback callback) override {}
  MOCK_METHOD0(NotificationEnabled, bool());
  MOCK_CONST_METHOD0(characteristic,
                     const bluetooth_v2_shlib::Gatt::Characteristic&());
  MOCK_CONST_METHOD0(uuid, const bluetooth_v2_shlib::Uuid&());
  MOCK_CONST_METHOD0(handle, uint16_t());
  MOCK_CONST_METHOD0(permissions, bluetooth_v2_shlib::Gatt::Permissions());
  MOCK_CONST_METHOD0(properties, bluetooth_v2_shlib::Gatt::Properties());

 private:
  ~MockRemoteCharacteristic();
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_
