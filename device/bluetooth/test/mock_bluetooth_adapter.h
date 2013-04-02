// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_

#include <string>

#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  class Observer : public BluetoothAdapter::Observer {
   public:
    Observer();
    virtual ~Observer();

    MOCK_METHOD2(AdapterPresentChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(AdapterPoweredChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(AdapterDiscoveringChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(DeviceAdded, void(BluetoothAdapter*, BluetoothDevice*));
    MOCK_METHOD2(DeviceChanged, void(BluetoothAdapter*, BluetoothDevice*));
    MOCK_METHOD2(DeviceRemoved, void(BluetoothAdapter*, BluetoothDevice*));
  };

  MockBluetoothAdapter();

  MOCK_METHOD1(AddObserver, void(BluetoothAdapter::Observer*));
  MOCK_METHOD1(RemoveObserver, void(BluetoothAdapter::Observer*));
  MOCK_CONST_METHOD0(address, std::string());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_CONST_METHOD0(IsPresent, bool());
  MOCK_CONST_METHOD0(IsPowered, bool());
  MOCK_METHOD3(SetPowered,
               void(bool discovering,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsDiscovering, bool());
  MOCK_METHOD2(StartDiscovering,
               void(const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD2(StopDiscovering,
               void(const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(GetDevices, BluetoothAdapter::ConstDeviceList());
  MOCK_METHOD1(GetDevice, BluetoothDevice*(const std::string& address));
  MOCK_CONST_METHOD1(GetDevice,
                     const BluetoothDevice*(const std::string& address));
  MOCK_METHOD2(
      ReadLocalOutOfBandPairingData,
      void(const BluetoothOutOfBandPairingDataCallback& callback,
           const ErrorCallback& error_callback));
 protected:
  virtual ~MockBluetoothAdapter();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
