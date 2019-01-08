// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_

#include <vector>

#include "base/observer_list.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/mock_remote_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockGattClientManager : public GattClientManager {
 public:
  MockGattClientManager();
  ~MockGattClientManager();

  void AddObserver(Observer* o) override { observers_.AddObserver(o); }
  void RemoveObserver(Observer* o) override { observers_.RemoveObserver(o); }

  MOCK_METHOD1(
      GetDevice,
      scoped_refptr<RemoteDevice>(const bluetooth_v2_shlib::Addr& addr));
  void GetDevice(
      const bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) override {
    std::move(cb).Run(GetDevice(addr));
  }

  MOCK_METHOD1(
      GetDeviceSync,
      scoped_refptr<RemoteDevice>(const bluetooth_v2_shlib::Addr& addr));

  MOCK_METHOD0(GetConnectedDevices, std::vector<scoped_refptr<RemoteDevice>>());
  void GetConnectedDevices(GetConnectDevicesCallback cb) {
    std::move(cb).Run(GetConnectedDevices());
  }

  MOCK_CONST_METHOD1(GetNumConnected,
                     void(base::OnceCallback<void(size_t)> cb));
  MOCK_METHOD1(NotifyConnect, void(const bluetooth_v2_shlib::Addr& addr));
  MOCK_METHOD1(NotifyBonded, void(const bluetooth_v2_shlib::Addr& addr));
  MOCK_METHOD0(task_runner, scoped_refptr<base::SingleThreadTaskRunner>());

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
