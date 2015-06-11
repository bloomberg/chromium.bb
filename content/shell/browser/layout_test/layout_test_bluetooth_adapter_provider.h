// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_

#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

namespace content {

// Implements fake adapters with named mock data set for use in tests as a
// result of layout tests calling testRunner.setBluetoothMockDataSet.
class LayoutTestBluetoothAdapterProvider {
 public:
  // Returns a BluetoothAdapter. Its behavior depends on |fake_adapter_name|.
  static scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter(
      const std::string& fake_adapter_name);

 private:
  // Returns "EmptyAdapter" fake BluetoothAdapter with the following
  // characteristics:
  //  - |StartDiscoverySession| runs the first argument with |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns an empty list of devices.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetEmptyAdapter();

  // Returns "SingleEmptyDeviceAdapter" fake BluetoothAdapter with the following
  // characteristics:
  //  - |StartDiscoverySession| runs the first argument with |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with an |EmptyDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetSingleEmptyDeviceAdapter();

  // Returns "ConnectableDeviceAdapter" fake BluetoothAdapter with the
  // following characteristics:
  //  - |StartDiscoverySession| runs the first argument with |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with a |ConnectableDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetConnectableDeviceAdapter();

  // Returns "UnconnectableDeviceAdapter" fake BluetoothAdapter with the
  // following characteristics:
  //  - |StartDiscoverySession| runs the first argument with |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with an |UnconnectableDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetUnconnectableDeviceAdapter();

  // Returns a fake |DiscoverySession| with the following characteristics:
  //  - |Stop| runs the first argument.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDiscoverySession>>
  GetDiscoverySession();

  // Returns an |EmptyDevice| with the following characeteristics:
  //  - |GetAddress| returns "Empty Mock Device instanceID".
  //  - |GetName| returns "Empty Mock Device name".
  //  - |GetBluetoothClass| returns 0x1F00.  "Unspecified Device Class": see
  //    bluetooth.org/en-us/specification/assigned-numbers/baseband
  //  - |GetVendorIDSource| returns |BluetoothDevice::VENDOR_ID_BLUETOOTH|.
  //  - |GetVendorID| returns 0xFFFF.
  //  - |GetProductID| returns 1.
  //  - |GetDeviceID| returns 2.
  //  - |IsPaired| returns true.
  //  - |GetUUIDs| returns a list with two UUIDs: "1800" and "1801".
  //  - |GetGattServices| returns a list with two services "Generic Access" and
  //    "Generic Attribute". "Generic Access" has a "Device Name" characteristic
  //    and "Generic Attribute" has a "Service Changed" characteristic.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetEmptyDevice(device::MockBluetoothAdapter* adapter);

  // Returns a fake |ConnectableDevice| with the same characteristics as
  // |EmptyDevice| except:
  //  - |CreateGattConnection| runs success callback with a
  //    fake BluetoothGattConnection as argument.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetConnectableDevice(device::MockBluetoothAdapter* adapter);

  // Returns a fake |UnconnectableDevice| with the same characteristics as
  // |EmptyDevice| except:
  //  - |CreateGattConnection| runs error callback with
  //    |BluetoothDevice::ERROR_FAILED| as argument.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetUnconnectableDevice(device::MockBluetoothAdapter* adapter);

  // Returns a fake BluetoothGattService with the following characteristics:
  // - |GetIdentifier| returns |uuid|.
  // - |GetUUID| returns BluetoothUUID(|uuid|).
  // - |IsLocal| returns false.
  // - |IsPrimary| returns true.
  // - |GetDevice| returns |device|.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetGattService(device::MockBluetoothDevice* device, const std::string& uuid);

  // Returns a fake BluetoothGattCharacteristic with the following
  // characteristics:
  // - |GetIdentifier| returns |uuid|.
  // - |GetUUID| returns BluetoothUUID(|uuid|).
  // - |IsLocal| returns false.
  // - |GetService| returns |service|.
  // - |IsNotifying| returns false.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetGattCharacteristic(device::MockBluetoothGattService* service,
                        const std::string& uuid);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
