// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_

#include <string>

#include "base/string16.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
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

  MOCK_CONST_METHOD0(GetBluetoothClass, uint32());
  MOCK_CONST_METHOD0(GetDeviceName, std::string());
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetVendorID, uint16());
  MOCK_CONST_METHOD0(GetProductID, uint16());
  MOCK_CONST_METHOD0(GetDeviceID, uint16());
  MOCK_CONST_METHOD0(GetName, string16());
  MOCK_CONST_METHOD0(GetDeviceType, BluetoothDevice::DeviceType());
  MOCK_CONST_METHOD0(IsPaired, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectable, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(GetServices, ServiceList());
  MOCK_METHOD2(GetServiceRecords,
               void(const BluetoothDevice::ServiceRecordsCallback&,
                    const BluetoothDevice::ErrorCallback&));
  MOCK_CONST_METHOD1(ProvidesServiceWithUUID, bool(const std::string&));
  MOCK_METHOD2(ProvidesServiceWithName,
               void(const std::string&,
                    const BluetoothDevice::ProvidesServiceCallback&));
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
  MOCK_METHOD2(ConnectToService,
               void(const std::string&,
                    const BluetoothDevice::SocketCallback&));
  MOCK_METHOD3(ConnectToProfile,
               void(BluetoothProfile*,
                    const base::Closure&,
                    const BluetoothDevice::ErrorCallback&));

  MOCK_METHOD3(SetOutOfBandPairingData,
      void(const BluetoothOutOfBandPairingData& data,
           const base::Closure& callback,
           const BluetoothDevice::ErrorCallback& error_callback));
  MOCK_METHOD2(ClearOutOfBandPairingData,
      void(const base::Closure& callback,
           const BluetoothDevice::ErrorCallback& error_callback));

 private:
  uint32 bluetooth_class_;
  std::string name_;
  std::string address_;
  BluetoothDevice::ServiceList service_list_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
