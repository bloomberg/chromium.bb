// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDiscoverySession;
using device::BluetoothGattConnection;
using device::BluetoothGattService;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothDiscoverySession;
using device::MockBluetoothGattCharacteristic;
using device::MockBluetoothGattConnection;
using device::MockBluetoothGattService;
using testing::Between;
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::ResultOf;
using testing::Return;
using testing::WithArgs;
using testing::_;

namespace {
// Bluetooth UUIDs suitable to pass to BluetoothUUID().
const char kBatteryServiceUUID[] = "180f";
const char kGenericAccessServiceUUID[] = "1800";
const char kGenericAttributeServiceUUID[] = "1801";
const char kGlucoseServiceUUID[] = "1808";
const char kHeartRateServiceUUID[] = "180d";
const char kDeviceNameUUID[] = "2a00";

// Invokes Run() on the k-th argument of the function with no arguments.
ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  return ::testing::get<k>(args).Run();
}

// Invokes Run() on the k-th argument of the function with 1 argument.
ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  return ::testing::get<k>(args).Run(p0);
}

// Invokes Run() on the k-th argument of the function with the result
// of |func| as an argument.
ACTION_TEMPLATE(RunCallbackWithResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(func)) {
  return ::testing::get<k>(args).Run(func());
}

// Function to iterate over the adapter's devices and return the one
// that matches the address.
ACTION_P(GetMockDevice, adapter) {
  std::string address = arg0;
  for (BluetoothDevice* device : adapter->GetMockDevices()) {
    if (device->GetAddress() == address)
      return device;
  }
  return NULL;
}

std::set<BluetoothUUID> GetUUIDs(
    const device::BluetoothDiscoveryFilter* filter) {
  std::set<BluetoothUUID> result;
  filter->GetUUIDs(result);
  return result;
};

}  // namespace

namespace content {

// static
scoped_refptr<BluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(
    const std::string& fake_adapter_name) {
  // Old Adapters
  if (fake_adapter_name == "SingleEmptyDeviceAdapter")
    return GetSingleEmptyDeviceAdapter();
  else if (fake_adapter_name == "ConnectableDeviceAdapter")
    return GetConnectableDeviceAdapter();
  // New adapters
  else if (fake_adapter_name == "BaseAdapter")
    return GetBaseAdapter();
  else if (fake_adapter_name == "ScanFilterCheckingAdapter")
    return GetScanFilterCheckingAdapter();
  else if (fake_adapter_name == "EmptyAdapter")
    return GetEmptyAdapter();
  else if (fake_adapter_name == "FailStartDiscoveryAdapter")
    return GetFailStartDiscoveryAdapter();
  else if (fake_adapter_name == "GlucoseHeartRateAdapter")
    return GetGlucoseHeartRateAdapter();
  else if (fake_adapter_name == "MissingServiceGenericAccessAdapter")
    return GetMissingServiceGenericAccessAdapter();
  else if (fake_adapter_name == "MissingCharacteristicGenericAccessAdapter")
    return GetMissingCharacteristicGenericAccessAdapter();
  else if (fake_adapter_name == "GenericAccessAdapter")
    return GetGenericAccessAdapter();
  else if (fake_adapter_name == "FailingConnectionsAdapter")
    return GetFailingConnectionsAdapter();
  else if (fake_adapter_name == "")
    return NULL;

  NOTREACHED() << fake_adapter_name;
  return NULL;
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetBaseAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(
      new NiceMock<MockBluetoothAdapter>());

  // Using Invoke allows the adapter returned from this method to be futher
  // modified and have devices added to it. The call to ::GetDevices will
  // invoke ::GetConstMockDevices, returning all devices added up to that time.
  ON_CALL(*adapter, GetDevices())
      .WillByDefault(
          Invoke(adapter.get(), &MockBluetoothAdapter::GetConstMockDevices));

  // The call to ::GetDevice will invoke GetMockDevice which returns a device
  // matching the address provided if the device was added to the mock.
  ON_CALL(*adapter, GetDevice(_)).WillByDefault(GetMockDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetScanFilterCheckingAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetBaseAdapter());

  // This fails the test with an error message listing actual and expected UUIDs
  // if StartDiscoverySessionWithFilter() is called with the wrong argument.
  EXPECT_CALL(
      *adapter,
      StartDiscoverySessionWithFilterRaw(
          ResultOf(&GetUUIDs, ElementsAre(BluetoothUUID(kGlucoseServiceUUID),
                                          BluetoothUUID(kHeartRateServiceUUID),
                                          BluetoothUUID(kBatteryServiceUUID))),
          _, _))
      .WillRepeatedly(RunCallbackWithResult<1 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

  // Any unexpected call results in the failure callback.
  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>());

  // We need to add a device otherwise requestDevice would reject.
  adapter->AddMockDevice(GetBatteryDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetFailStartDiscoveryAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetBaseAdapter());

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>());

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetBaseAdapter());

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallbackWithResult<1 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

  return adapter.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDiscoverySession>>
LayoutTestBluetoothAdapterProvider::GetDiscoverySession() {
  scoped_ptr<NiceMock<MockBluetoothDiscoverySession>> discovery_session(
      new NiceMock<MockBluetoothDiscoverySession>());

  ON_CALL(*discovery_session, Stop(_, _))
      .WillByDefault(RunCallback<0 /* success_callback */>());

  return discovery_session.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetGlucoseHeartRateAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));
  adapter->AddMockDevice(GetGlucoseDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetMissingServiceGenericAccessAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetGenericAccessDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::
    GetMissingCharacteristicGenericAccessAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  scoped_ptr<NiceMock<MockBluetoothDevice>> device(
      GetGenericAccessDevice(adapter.get()));

  scoped_ptr<NiceMock<MockBluetoothGattService>> generic_access(
      GetBaseGATTService(device.get(), kGenericAccessServiceUUID));

  // Intentionally NOT adding a characteristic to generic_access service.

  device->AddMockService(generic_access.Pass());

  adapter->AddMockDevice(device.Pass());

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetGenericAccessAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  scoped_ptr<NiceMock<MockBluetoothDevice>> device(
      GetGenericAccessDevice(adapter.get()));

  scoped_ptr<NiceMock<MockBluetoothGattService>> generic_access(
      GetBaseGATTService(device.get(), kGenericAccessServiceUUID));

  scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>> device_name(
      GetBaseGATTCharacteristic(generic_access.get(), kDeviceNameUUID));

  // Read response.
  std::string device_name_str = device->GetDeviceName();
  std::vector<uint8_t> device_name_value(device_name_str.begin(),
                                         device_name_str.end());

  ON_CALL(*device_name, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallback<0>(device_name_value));

  // Write response.
  ON_CALL(*device_name, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<1 /* sucess callback */>());

  generic_access->AddMockCharacteristic(device_name.Pass());
  device->AddMockService(generic_access.Pass());
  adapter->AddMockDevice(device.Pass());

  return adapter.Pass();
}

scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetFailingConnectionsAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  for (int error = BluetoothDevice::ERROR_UNKNOWN;
       error <= BluetoothDevice::ERROR_UNSUPPORTED_DEVICE; error++) {
    adapter->AddMockDevice(GetUnconnectableDevice(
        adapter.get(), static_cast<BluetoothDevice::ConnectErrorCode>(error)));
  }
  return adapter.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetBaseDevice(
    MockBluetoothAdapter* adapter,
    const std::string& device_name,
    device::BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> device(
      new NiceMock<MockBluetoothDevice>(adapter, 0x1F00 /* Bluetooth class */,
                                        device_name, address, true /* paired */,
                                        true /* connected */));

  ON_CALL(*device, GetUUIDs()).WillByDefault(Return(uuids));

  // Using Invoke allows the device returned from this method to be futher
  // modified and have more services added to it. The call to ::GetGattServices
  // will invoke ::GetMockServices, returning all services added up to that
  // time.
  ON_CALL(*device, GetGattServices())
      .WillByDefault(
          Invoke(device.get(), &MockBluetoothDevice::GetMockServices));

  // The call to BluetoothDevice::GetGattService will invoke ::GetMockService
  // which returns a service matching the identifier provided if the service
  // was added to the mock.
  ON_CALL(*device, GetGattService(_))
      .WillByDefault(
          Invoke(device.get(), &MockBluetoothDevice::GetMockService));

  ON_CALL(*device, GetVendorIDSource())
      .WillByDefault(Return(BluetoothDevice::VENDOR_ID_BLUETOOTH));
  ON_CALL(*device, GetVendorID()).WillByDefault(Return(0xFFFF));
  ON_CALL(*device, GetProductID()).WillByDefault(Return(1));
  ON_CALL(*device, GetDeviceID()).WillByDefault(Return(2));

  return device.Pass();
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetBatteryDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kBatteryServiceUUID));

  return GetBaseDevice(adapter, "Battery Device", uuids, "00:00:00:00:01");
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetGlucoseDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kGlucoseServiceUUID));

  return GetBaseDevice(adapter, "Glucose Device", uuids, "00:00:00:00:02");
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetHeartRateDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));

  return GetBaseDevice(adapter, "Heart Rate Device", uuids, "00:00:00:00:03");
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetConnectableDeviceNew(
    device::MockBluetoothAdapter* adapter,
    const std::string& device_name,
    BluetoothDevice::UUIDList uuids) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> device(
      GetBaseDevice(adapter, device_name, uuids));

  BluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(
          RunCallbackWithResult<0 /* success_callback */>([device_ptr]() {
            return make_scoped_ptr(new NiceMock<MockBluetoothGattConnection>(
                device_ptr->GetAddress()));
          }));

  return device.Pass();
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetUnconnectableDevice(
    MockBluetoothAdapter* adapter,
    BluetoothDevice::ConnectErrorCode error_code,
    const std::string& device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorUUID(error_code)));

  scoped_ptr<testing::NiceMock<MockBluetoothDevice>> device(
      GetBaseDevice(adapter, device_name, uuids, makeMACAddress(error_code)));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  return device.Pass();
}

// static
scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetGenericAccessDevice(
    MockBluetoothAdapter* adapter,
    const std::string& device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));

  return GetConnectableDeviceNew(adapter, device_name, uuids);
}

// static
scoped_ptr<NiceMock<MockBluetoothGattService>>
LayoutTestBluetoothAdapterProvider::GetBaseGATTService(
    MockBluetoothDevice* device,
    const std::string& uuid) {
  scoped_ptr<NiceMock<MockBluetoothGattService>> service(
      new NiceMock<MockBluetoothGattService>(
          device, uuid /* identifier */, BluetoothUUID(uuid),
          true /* is_primary */, false /* is_local */));

  ON_CALL(*service, GetCharacteristics())
      .WillByDefault(Invoke(service.get(),
                            &MockBluetoothGattService::GetMockCharacteristics));

  ON_CALL(*service, GetCharacteristic(_))
      .WillByDefault(Invoke(service.get(),
                            &MockBluetoothGattService::GetMockCharacteristic));

  return service.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>>
LayoutTestBluetoothAdapterProvider::GetBaseGATTCharacteristic(
    MockBluetoothGattService* service,
    const std::string& uuid) {
  return make_scoped_ptr(new NiceMock<MockBluetoothGattCharacteristic>(
      service, uuid + " Identifier", BluetoothUUID(uuid), false /* is_local */,
      NULL /* properties */, NULL /* permissions */));
}

// static
std::string LayoutTestBluetoothAdapterProvider::errorUUID(uint32_t alias) {
  return base::StringPrintf("%08x-97e5-4cd7-b9f1-f5a427670c59", alias);
}

// static
std::string LayoutTestBluetoothAdapterProvider::makeMACAddress(uint64_t addr) {
  return BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012" PRIx64, addr));
}

// The functions after this haven't been updated to the new design yet.

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetSingleEmptyDeviceAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetEmptyDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetConnectableDeviceAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetConnectableDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetEmptyDevice(
    MockBluetoothAdapter* adapter,
    const std::string& device_name) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> empty_device(
      new NiceMock<MockBluetoothDevice>(
          adapter, 0x1F00 /* Bluetooth Class */, device_name,
          "00:00:00:00:00" /* Device Address */, true /* Paired */,
          true /* Connected */));

  ON_CALL(*empty_device, GetVendorIDSource())
      .WillByDefault(Return(BluetoothDevice::VENDOR_ID_BLUETOOTH));
  ON_CALL(*empty_device, GetVendorID()).WillByDefault(Return(0xFFFF));
  ON_CALL(*empty_device, GetProductID()).WillByDefault(Return(1));
  ON_CALL(*empty_device, GetDeviceID()).WillByDefault(Return(2));

  BluetoothDevice::UUIDList list;
  list.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  list.push_back(BluetoothUUID(kGenericAttributeServiceUUID));
  ON_CALL(*empty_device, GetUUIDs()).WillByDefault(Return(list));

  scoped_ptr<NiceMock<MockBluetoothGattService>> generic_access(
      GetBaseGATTService(empty_device.get(), kGenericAccessServiceUUID));
  scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>>
      device_name_characteristic(GetBaseGATTCharacteristic(
          generic_access.get(), "2A00" /* Device Name */));

  std::vector<uint8_t> device_name_value(device_name.begin(),
                                         device_name.end());
  ON_CALL(*device_name_characteristic, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallback<0>(device_name_value));

  ON_CALL(*device_name_characteristic, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<2 /* error callback */>(
          BluetoothGattService::GATT_ERROR_NOT_PERMITTED));

  generic_access->AddMockCharacteristic(device_name_characteristic.Pass());

  scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>> reconnection_address(
      GetBaseGATTCharacteristic(generic_access.get(),
                                "2A03" /* Reconnection Address */));

  ON_CALL(*reconnection_address, ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          RunCallback<1>(BluetoothGattService::GATT_ERROR_NOT_PERMITTED));

  ON_CALL(*reconnection_address, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<1 /* success callback */>());

  generic_access->AddMockCharacteristic(reconnection_address.Pass());

  empty_device->AddMockService(generic_access.Pass());

  // Using Invoke allows the device returned from this method to be futher
  // modified and have more services added to it. The call to ::GetGattServices
  // will invoke ::GetMockServices, returning all services added up to that
  // time.
  ON_CALL(*empty_device, GetGattServices())
      .WillByDefault(
          Invoke(empty_device.get(), &MockBluetoothDevice::GetMockServices));

  // The call to BluetoothDevice::GetGattService will invoke ::GetMockService
  // which returns a service matching the identifier provided if the service
  // was added to the mock.
  ON_CALL(*empty_device, GetGattService(_))
      .WillByDefault(
          Invoke(empty_device.get(), &MockBluetoothDevice::GetMockService));

  return empty_device.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetConnectableDevice(
    MockBluetoothAdapter* adapter) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> device(GetEmptyDevice(adapter));

  BluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(
          RunCallbackWithResult<0 /* success_callback */>([device_ptr]() {
            return make_scoped_ptr(new NiceMock<MockBluetoothGattConnection>(
                device_ptr->GetAddress()));
          }));

  return device.Pass();
}

}  // namespace content
