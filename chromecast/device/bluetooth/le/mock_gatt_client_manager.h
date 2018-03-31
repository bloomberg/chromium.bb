// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_

#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockGattClientManager : public GattClientManager {
 public:
  MockGattClientManager();
  ~MockGattClientManager();

  MOCK_METHOD1(AddObserver, void(Observer* o));
  MOCK_METHOD1(RemoveObserver, void(Observer* o));
  void GetDevice(
      const bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) override {}
  MOCK_METHOD1(
      GetDeviceSync,
      scoped_refptr<RemoteDevice>(const bluetooth_v2_shlib::Addr& addr));
  MOCK_CONST_METHOD0(GetNumConnected, size_t());
  MOCK_METHOD1(NotifyConnect, void(const bluetooth_v2_shlib::Addr& addr));
  MOCK_METHOD0(task_runner, scoped_refptr<base::SingleThreadTaskRunner>());
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
