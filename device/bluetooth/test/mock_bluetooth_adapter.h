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

  virtual bool IsInitialized() const { return true; }

  MOCK_METHOD1(AddObserver, void(BluetoothAdapter::Observer*));
  MOCK_METHOD1(RemoveObserver, void(BluetoothAdapter::Observer*));
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_METHOD3(SetName,
               void(const std::string& name,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsPresent, bool());
  MOCK_CONST_METHOD0(IsPowered, bool());
  MOCK_METHOD3(SetPowered,
               void(bool powered,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsDiscoverable, bool());
  MOCK_METHOD3(SetDiscoverable,
               void(bool discoverable,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsDiscovering, bool());
  MOCK_METHOD2(StartDiscoverySession,
               void(const DiscoverySessionCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(GetDevices, BluetoothAdapter::ConstDeviceList());
  MOCK_METHOD1(GetDevice, BluetoothDevice*(const std::string& address));
  MOCK_CONST_METHOD1(GetDevice,
                     const BluetoothDevice*(const std::string& address));
  MOCK_METHOD2(AddPairingDelegate,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    enum PairingDelegatePriority priority));
  MOCK_METHOD1(RemovePairingDelegate,
               void(BluetoothDevice::PairingDelegate* pairing_delegate));
  MOCK_METHOD0(DefaultPairingDelegate, BluetoothDevice::PairingDelegate*());
  MOCK_METHOD4(CreateRfcommService,
               void(const BluetoothUUID& uuid,
                    const ServiceOptions& options,
                    const CreateServiceCallback& callback,
                    const CreateServiceErrorCallback& error_callback));
  MOCK_METHOD4(CreateL2capService,
               void(const BluetoothUUID& uuid,
                    const ServiceOptions& options,
                    const CreateServiceCallback& callback,
                    const CreateServiceErrorCallback& error_callback));

 protected:
  virtual void AddDiscoverySession(const base::Closure& callback,
                                   const ErrorCallback& error_callback);
  virtual void RemoveDiscoverySession(const base::Closure& callback,
                                      const ErrorCallback& error_callback);
  virtual ~MockBluetoothAdapter();

  MOCK_METHOD1(RemovePairingDelegateInternal,
               void(BluetoothDevice::PairingDelegate* pairing_delegate));
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
