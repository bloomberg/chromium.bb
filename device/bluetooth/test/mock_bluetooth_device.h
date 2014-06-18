// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_

#include <string>

#include "base/strings/string16.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothAdapter;

class MockBluetoothDevice : public BluetoothDevice {
 public:
  MockBluetoothDevice(MockBluetoothAdapter* adapter,
                      uint32 bluetooth_class,
                      const std::string& name,
                      const std::string& address,
                      bool paired,
                      bool connected);
  virtual ~MockBluetoothDevice();

  MOCK_METHOD1(AddObserver, void(BluetoothDevice::Observer*));
  MOCK_METHOD1(RemoveObserver, void(BluetoothDevice::Observer*));
  MOCK_CONST_METHOD0(GetBluetoothClass, uint32());
  MOCK_CONST_METHOD0(GetDeviceName, std::string());
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetVendorIDSource, BluetoothDevice::VendorIDSource());
  MOCK_CONST_METHOD0(GetVendorID, uint16());
  MOCK_CONST_METHOD0(GetProductID, uint16());
  MOCK_CONST_METHOD0(GetDeviceID, uint16());
  MOCK_CONST_METHOD0(GetName, base::string16());
  MOCK_CONST_METHOD0(GetDeviceType, BluetoothDevice::DeviceType());
  MOCK_CONST_METHOD0(GetRSSI, int());
  MOCK_CONST_METHOD0(GetCurrentHostTransmitPower, int());
  MOCK_CONST_METHOD0(GetMaximumHostTransmitPower, int());
  MOCK_CONST_METHOD0(IsPaired, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectable, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(GetUUIDs, UUIDList());
  MOCK_CONST_METHOD0(ExpectingPinCode, bool());
  MOCK_CONST_METHOD0(ExpectingPasskey, bool());
  MOCK_CONST_METHOD0(ExpectingConfirmation, bool());
  MOCK_METHOD3(Connect,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    const base::Closure& callback,
                    const BluetoothDevice::ConnectErrorCallback&
                        error_callback));
  MOCK_METHOD1(SetPinCode, void(const std::string&));
  MOCK_METHOD1(SetPasskey, void(uint32));
  MOCK_METHOD0(ConfirmPairing, void());
  MOCK_METHOD0(RejectPairing, void());
  MOCK_METHOD0(CancelPairing, void());
  MOCK_METHOD2(Disconnect,
               void(const base::Closure& callback,
                    const BluetoothDevice::ErrorCallback& error_callback));
  MOCK_METHOD1(Forget, void(const BluetoothDevice::ErrorCallback&));
  MOCK_METHOD3(ConnectToService,
               void(const BluetoothUUID& uuid,
                    const ConnectToServiceCallback& callback,
                    const ConnectToServiceErrorCallback& error_callback));
  MOCK_METHOD2(CreateGattConnection,
               void(const GattConnectionCallback& callback,
                    const ConnectErrorCallback& error_callback));

  MOCK_METHOD2(StartConnectionMonitor,
               void(const base::Closure& callback,
                    const BluetoothDevice::ErrorCallback& error_callback));

  MOCK_CONST_METHOD0(GetGattServices, std::vector<BluetoothGattService*>());
  MOCK_CONST_METHOD1(GetGattService, BluetoothGattService*(const std::string&));

 private:
  uint32 bluetooth_class_;
  std::string name_;
  std::string address_;
  BluetoothDevice::UUIDList uuids_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
