// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_

#include <stdint.h>

#include "base/callback.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

namespace content {

// Implements fake adapters with named mock data set for use in tests as a
// result of layout tests calling testRunner.setBluetoothMockDataSet.

// An adapter named 'FooAdapter' in
// https://webbluetoothcg.github.io/web-bluetooth/tests/ is provided by a
// corresponding 'GetFooAdapter' function, and re-documented here to capture
// differences between our implementations.

class LayoutTestBluetoothAdapterProvider {
 public:
  // Returns a BluetoothAdapter. Its behavior depends on |fake_adapter_name|.
  static scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter(
      const std::string& fake_adapter_name);

 private:
  // Adapters

  // |BaseAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - GetDevices:
  //      Returns a list of devices added to the adapter.
  //  - GetDevice:
  //      Returns a device matching the address provided if the device was
  //      added to the adapter.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetBaseAdapter();

  // |PresentAdapter|
  // Inherits from |BaseAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPresent: Returns true
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPresentAdapter();

  // |NotPresentAdapter|
  // Inherits from |BaseAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPresent: Returns false
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetNotPresentAdapter();

  // |PoweredAdapter|
  // Inherits from |PresentAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPowered: Returns true
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPoweredAdapter();

  // |NotPoweredAdapter|
  // Inherits from |PresentAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPowered: Returns false
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetNotPoweredAdapter();

  // |ScanFilterCheckingAdapter|
  // Inherits from |PoweredAdapter|
  // BluetoothAdapter that asserts that its StartDiscoverySessionWithFilter()
  // method is called with a filter consisting of the standard battery, heart
  // rate, and glucose services.
  // Devices added:
  //  - |BatteryDevice|
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      - With correct arguments: Run success callback.
  //      - With incorrect arguments: Mock complains that function with
  //        correct arguments was never called and error callback is called.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetScanFilterCheckingAdapter();

  // |FailStartDiscoveryAdapter|
  // Inherits from |PoweredAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run error callback.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailStartDiscoveryAdapter();

  // |EmptyAdapter|
  // Inherits from |PoweredAdapter|
  // Devices Added:
  //  None.
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run success callback with |DiscoverySession|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetEmptyAdapter();

  // |PowerValueAdapter|(tx_power, rssi)
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //  - |HeartRateDevice|
  //    - Mock Functions:
  //      - GetInquiryTxPower(): Returns tx_power
  //      - GetInquiryRSSI(): Returns rssi
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPowerValueAdapter(int8_t tx_power, int8_t rssi);

  // |PowerPresenceAdapter|(tx_power_present, rssi_present)
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //  - |HeartRateDevice|
  //    - Mock Functions:
  //      - GetInquiryTxPower(): If tx_power_present is true, returns -10,
  //        the TxPower of a device broadcasting at 0.1mW. Otherwise
  //        returns 127 which denotes a missing Tx Power.
  //        TODO(ortuno): Change 127 to -128 when Tx Power Unknown value gets
  //        fixed: http://crbug.com/551572
  //      - GetInquiryRSSI(): If rssi_present is true returns -51,
  //        the RSSI at 1m from a device broadcasting at 0.1mW. Otherwise
  //        returns 127 which denotes a missing RSSI.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPowerPresenceAdapter(bool tx_power_present, bool rssi_present);

  // |GlucoseHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Devices added:
  //  - |GlucoseDevice|
  //  - |HeartRateDevice|
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetGlucoseHeartRateAdapter();

  // |GetUnicodeDeviceAdapter|
  // Inherits from |EmptyAdapter|
  // Internal structure
  //  - UnicodeDevice
  //    - Mock Functions:
  //      - GetName(): Returns "❤❤❤❤❤❤❤❤❤"
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetUnicodeDeviceAdapter();

  // |SecondDiscoveryFindsHeartRateAdapter|
  // Inherits from |PoweredAdapter|
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run success callback with |DiscoverySession|.
  //      After the first call, adds a |HeartRateDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetSecondDiscoveryFindsHeartRateAdapter();

  // |MissingServiceHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - Advertised UUIDs:
  //         - Heart Rate UUID (0x180d)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMissingServiceHeartRateAdapter();

  // |MissingCharacteristicHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // The services in this adapter do not contain any characteristics.
  // Internal Structure:
  //   - Heart Rate Device
  //      - Advertised UUIDs:
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service
  //         - Heart Rate Service
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMissingCharacteristicHeartRateAdapter();

  // |HeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - Advertised UUIDs:
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetHeartRateAdapter();

  // |HeartRateAndHIDAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - |ConnectableDevice|(adapter, "Heart Rate And HID Device", uuids)
  //      - Advertised UUIDs:
  //         - Heart Rate UUID (0x180d)
  //         - Human Interface Device UUID (0x1812) (a blacklisted service)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  //         - Human Interface Device Service - No characteristics needed
  //           because the service is blacklisted.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetHeartRateAndHIDAdapter();

  // |DelayedServicesDiscoveryAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - Generic Access UUID (0x1800)
  //      - Heart Rate UUID (0x180D)
  //      - Heart Rate Service (No services will be returned the first time
  //                            GetServices is called. Subsequent calls will
  //                            return the Heart Rate Service)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDelayedServicesDiscoveryAdapter();

  // |FailingConnectionsAdapter|
  // Inherits from |EmptyAdapter|
  // FailingConnectionsAdapter holds a device for each type of connection error
  // that can occur. This way we don’t need to create an adapter for each type
  // of error. Each of the devices has a service with a different UUID so that
  // they can be accessed by using different filters.
  // See connectErrorUUID() declaration below.
  // Internal Structure:
  //  - UnconnectableDevice(BluetoothDevice::ERROR_UNKNOWN)
  //    connectErrorUUID(0x0)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_INPROGRESS)
  //    connectErrorUUID(0x1)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_FAILED)
  //    connectErrorUUID(0x2)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_FAILED)
  //    connectErrorUUID(0x3)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_CANCELED)
  //    connectErrorUUID(0x4)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_REJECTED)
  //    connectErrorUUID(0x5)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_TIMEOUT)
  //    connectErrorUUID(0x6)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE)
  //    connectErrorUUID(0x7)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_ATTRIBUTE_LENGTH_INVALID)
  //    connectErrorUUID(0x8)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_CONNECTION_CONGESTED)
  //    connectErrorUUID(0x9)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_INSUFFICIENT_ENCRYPTION)
  //    connectErrorUUID(0xa)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_OFFSET_INVALID)
  //    connectErrorUUID(0xb)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_READ_NOT_PERMITTED)
  //    connectErrorUUID(0xc)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_REQUEST_NOT_SUPPORTED)
  //    connectErrorUUID(0xd)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_WRITE_NOT_PERMITTED)
  //    connectErrorUUID(0xe)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailingConnectionsAdapter();

  // |FailingGATTOperationsAdapter|
  // Inherits from |EmptyAdapter|
  // FailingGATTOperationsAdapter holds a device with one
  // service: ErrorsService. This service contains a characteristic for each
  // type of GATT Error that can be thrown. Trying to write or read from these
  // characteristics results in the corresponding error being returned.
  // GetProperties returns the following for all characteristics:
  // (BluetoothGattCharacteristic::PROPERTY_READ |
  // BluetoothGattCharacteristic::PROPERTY_WRITE |
  // BluetoothGattCharacteristic::PROPERTY_INDICATE)
  // Internal Structure:
  //   - ErrorsDevice
  //      - ErrorsService errorUUID(0xA0)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_UNKNOWN)
  //              errorUUID(0xA1)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_FAILED)
  //              errorUUID(0xA2)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_IN_PROGRESS)
  //              errorUUID(0xA3)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_INVALID_LENGTH)
  //              errorUUID(0xA4)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_NOT_PERMITTED)
  //              errorUUID(0xA5)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED)
  //              errorUUID(0xA6)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_NOT_PAIRED)
  //              errorUUID(0xA7)
  //          - ErrorCharacteristic(
  //              BluetoothGattService::GATT_ERROR_NOT_SUPPORTED)
  //              errorUUID(0xA8)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailingGATTOperationsAdapter();

  // Discovery Sessions

  // |DiscoverySession|
  // Mock Functions:
  //  - Stop:
  //      Run success callback.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDiscoverySession>>
  GetDiscoverySession();

  // Devices

  // |BaseDevice|
  // Adv UUIDs added:
  // None.
  // Services added:
  // None.
  // MockFunctions:
  //  - GetUUIDs:
  //      Returns uuids
  //  - GetGattServices:
  //      Returns a list of all services added to the device.
  //  - GetGattService:
  //      Return a service matching the identifier provided if the service was
  //      added to the mock.
  //  - GetAddress:
  //      Returns: address
  //  - GetName:
  //      Returns: device_name.
  //  - GetBluetoothClass:
  //      Returns: 0x1F00. “Unspecified Device Class” see
  //      bluetooth.org/en-us/specification/assigned-numbers/baseband
  //  - GetVendorIDSource:
  //      Returns: BluetoothDevice::VENDOR_ID_BLUETOOTH.
  //  - GetVendorID:
  //      Returns: 0xFFFF.
  //  - GetProductID:
  //      Returns: 1.
  //  - GetDeviceID:
  //      Returns: 2.
  //  - IsPaired:
  //      Returns true.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetBaseDevice(device::MockBluetoothAdapter* adapter,
                const std::string& device_name = "Base Device",
                device::BluetoothDevice::UUIDList uuids =
                    device::BluetoothDevice::UUIDList(),
                const std::string& address = "00:00:00:00:00:00");

  // |BatteryDevice|
  // Inherits from |BaseDevice|(adapter, "Battery Device", uuids,
  //                            "00:00:00:00:00:01")
  // Adv UUIDs added:
  //   - Generic Access (0x1800)
  //   - Battery Service UUID (0x180F)
  // Services added:
  // None.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetBatteryDevice(device::MockBluetoothAdapter* adapter);

  // |GlucoseDevice|
  // Inherits from |BaseDevice|(adapter, "Glucose Device", uuids,
  //                            "00:00:00:00:00:02")
  // Adv UUIDs added:
  //   - Generic Access (0x1800)
  //   - Glucose UUID (0x1808)
  // Services added:
  // None.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetGlucoseDevice(device::MockBluetoothAdapter* adapter);

  // |ConnectableDevice|
  // Inherits from |BaseDevice|(adapter, device_name)
  // Adv UUIDs added:
  // None.
  // Services added:
  // None.
  // Mock Functions:
  //   - CreateGattConnection:
  //       - Run success callback with BaseGATTConnection
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetConnectableDevice(
      device::MockBluetoothAdapter* adapter,
      const std::string& device_name = "Connectable Device",
      device::BluetoothDevice::UUIDList = device::BluetoothDevice::UUIDList(),
      const std::string& address = "00:00:00:00:00:00");

  // |UnconnectableDevice|
  // Inherits from |BaseDevice|(adapter, device_name)
  // Adv UUIDs added:
  //  - errorUUID(error_code)
  // Services added:
  // None.
  // Mock Functions:
  //  - CreateGATTConnection:
  //      - Run error callback with error_type
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetUnconnectableDevice(
      device::MockBluetoothAdapter* adapter,
      device::BluetoothDevice::ConnectErrorCode error_code,
      const std::string& device_name = "Unconnectable Device");

  // |HeartRateDevice|
  // Inherits from |ConnectableDevice|(adapter, "Heart Rate Device", uuids)
  // Adv UUIDs added:
  //   - Heart Rate UUID (0x180D)
  // Services added:
  // None. Each user of the HeartRateDevice is in charge of adding the
  // relevant services, characteristics and descriptors.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetHeartRateDevice(device::MockBluetoothAdapter* adapter);

  // Services

  // |BaseGATTService|
  // Characteristics added:
  // None.
  // Mock Functions:
  //   - GetCharacteristics:
  //       Returns a list with all the characteristics added to the service
  //   - GetCharacteristic:
  //       Returns a characteristic matching the identifier provided if the
  //       characteristic was added to the mock.
  //   - GetIdentifier:
  //       Returns: uuid + “ Identifier”
  //   - GetUUID:
  //       Returns: uuid
  //   - IsLocal:
  //       Returns: false
  //   - IsPrimary:
  //       Returns: true
  //   - GetDevice:
  //       Returns: device
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetBaseGATTService(device::MockBluetoothDevice* device,
                     const std::string& uuid);

  // |GenericAccessService|
  // Internal Structure:
  //  - Characteristics:
  //     - Device Name:
  //        - Mock Functions:
  //           - Read: Calls success callback with device's name.
  //           - Write: Calls success callback.
  //           - GetProperties: Returns
  //               BluetoothGattCharacteristic::PROPERTY_READ |
  //               BluetoothGattCharacteristic::PROPERTY_WRITE
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetGenericAccessService(device::MockBluetoothAdapter* adapter,
                          device::MockBluetoothDevice* device);

  // |HeartRateService|
  // Internal Structure:
  //  - Characteristics:
  //     - Heart Rate Measurement (0x2a37)
  //        - Mock Functions:
  //           - StartNotifySession: Sets a timer to call
  //               GattCharacteristicValueChanged every 10ms and calls
  //               success callback with a
  //               BaseGATTNotifySession(characteristic_instance_id)
  //               TODO: Instead of a timer we should be able to tell the fake
  //               to call GattCharacteristicValueChanged from js.
  //               https://crbug.com/543884
  //           - GetProperties: Returns
  //               BluetoothGattCharacteristic::PROPERTY_NOTIFY
  //     - Body Sensor Location (0x2a38)
  //        - Mock Functions:
  //           - Read: Calls GattCharacteristicValueChanged and success
  //               callback with [1] which corresponds to chest.
  //           - GetProperties: Returns
  //               BluetoothGattCharacteristic::PROPERTY_READ
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetHeartRateService(device::MockBluetoothAdapter* adapter,
                      device::MockBluetoothDevice* device);

  // Characteristics

  // |BaseCharacteristic|(service, uuid)
  // Descriptors added:
  // None.
  // Mock Functions:
  //   - TODO(ortuno): http://crbug.com/483347 GetDescriptors:
  //       Returns: all descriptors added to the characteristic
  //   - TODO(ortuno): http://crbug.com/483347 GetDescriptor:
  //       Returns the descriptor matching the identifier provided if the
  //       descriptor was added to the characteristic.
  //   - GetIdentifier:
  //       Returns: uuid + “ Identifier”
  //   - GetUUID:
  //       Returns: uuid
  //   - IsLocal:
  //       Returns: false
  //   - GetService:
  //       Returns: service
  //   - GetProperties:
  //       Returns: NULL
  //   - GetPermissions:
  //       Returns: NULL
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetBaseGATTCharacteristic(
      device::MockBluetoothGattService* service,
      const std::string& uuid,
      device::BluetoothGattCharacteristic::Properties properties);

  // |ErrorCharacteristic|(service, error_type)
  // Inherits from BaseCharacteristic(service, errorUUID(error_type + 0xA1))
  // Descriptors added:
  // None.
  // Mock Functions:
  //   - ReadRemoteCharacteristic:
  //       Run error callback with error_type
  //   - WriteRemoteCharacteristic:
  //       Run error callback with error_type
  //   - StartNotifySession:
  //       Run error callback with error_type
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetErrorCharacteristic(
      device::MockBluetoothGattService* service,
      device::BluetoothGattService::GattErrorCode error_code);

  // Notify Sessions

  // |BaseGATTNotifySession|(characteristic_identifier)
  // Mock Functions:
  //   - GetCharacteristicIdentifier:
  //       Returns: characteristic_identifier
  //   - IsActive:
  //       Returns: true
  //   - Stop:
  //       Stops calling GattCharacteristicValueChanged and runs callback.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattNotifySession>>
  GetBaseGATTNotifySession(const std::string& characteristic_identifier);

  // Helper functions:

  // DEPRECATED: This is a poor practice as it exposes the specific
  //             enum values of this code base into the UUIDs used
  //             by the test data. Prefer methods such as
  //             connectErrorUUID.
  // errorUUID(alias) returns a UUID with the top 32 bits of
  // "00000000-97e5-4cd7-b9f1-f5a427670c59" replaced with the bits of |alias|.
  // For example, errorUUID(0xDEADBEEF) returns
  // "deadbeef-97e5-4cd7-b9f1-f5a427670c59". The bottom 96 bits of error UUIDs
  // were generated as a type 4 (random) UUID.
  static std::string errorUUID(uint32_t alias);

  // Returns a stable test data UUID associated with a given
  // BluetoothDevice::ConnectErrorCode.
  static device::BluetoothUUID connectErrorUUID(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Function to turn an integer into an MAC address of the form
  // XX:XX:XX:XX:XX:XX. For example makeMACAddress(0xdeadbeef)
  // returns "00:00:DE:AD:BE:EF".
  static std::string makeMACAddress(uint64_t addr);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
