// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

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
using testing::Invoke;
using testing::Return;
using testing::NiceMock;
using testing::_;

namespace {
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

}  // namespace

namespace content {

// static
scoped_refptr<BluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(
    const std::string& fake_adapter_name) {
  if (fake_adapter_name == "EmptyAdapter")
    return GetEmptyAdapter();
  else if (fake_adapter_name == "SingleEmptyDeviceAdapter")
    return GetSingleEmptyDeviceAdapter();
  else if (fake_adapter_name == "ConnectableDeviceAdapter")
    return GetConnectableDeviceAdapter();
  else if (fake_adapter_name == "UnconnectableDeviceAdapter")
    return GetUnconnectableDeviceAdapter();
  else if (fake_adapter_name == "")
    return NULL;

  NOTREACHED();
  return NULL;
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(
      new NiceMock<MockBluetoothAdapter>());

  ON_CALL(*adapter, StartDiscoverySession(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

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
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetUnconnectableDeviceAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetUnconnectableDevice(adapter.get()));

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
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetEmptyDevice(
    MockBluetoothAdapter* adapter) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> empty_device(
      new NiceMock<MockBluetoothDevice>(
          adapter, 0x1F00 /* Bluetooth Class */, "Empty Mock Device name",
          "Empty Mock Device instanceID", true /* Paired */,
          true /* Connected */));

  ON_CALL(*empty_device, GetVendorIDSource())
      .WillByDefault(Return(BluetoothDevice::VENDOR_ID_BLUETOOTH));
  ON_CALL(*empty_device, GetVendorID()).WillByDefault(Return(0xFFFF));
  ON_CALL(*empty_device, GetProductID()).WillByDefault(Return(1));
  ON_CALL(*empty_device, GetDeviceID()).WillByDefault(Return(2));

  BluetoothDevice::UUIDList list;
  list.push_back(BluetoothUUID("1800"));
  list.push_back(BluetoothUUID("1801"));
  ON_CALL(*empty_device, GetUUIDs()).WillByDefault(Return(list));

  scoped_ptr<NiceMock<MockBluetoothGattService>> generic_access(
      GetGattService(empty_device.get(), "1800" /* Generic Access */));
  scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>> device_name(
      GetGattCharacteristic(generic_access.get(), "2A00" /* Device Name */));

  std::string value_str("Empty Mock Device name");
  std::vector<uint8_t> value(value_str.begin(), value_str.end());
  ON_CALL(*device_name, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallback<0>(value));

  generic_access->AddMockCharacteristic(device_name.Pass());

  scoped_ptr<NiceMock<MockBluetoothGattCharacteristic>> reconnection_address(
      GetGattCharacteristic(generic_access.get(),
                            "2A03" /* Reconnection Address */));

  ON_CALL(*reconnection_address, ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          RunCallback<1>(BluetoothGattService::GATT_ERROR_NOT_PERMITTED));

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

// static
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetUnconnectableDevice(
    MockBluetoothAdapter* adapter) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> device(GetEmptyDevice(adapter));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(
          RunCallback<1 /* error_callback */>(BluetoothDevice::ERROR_FAILED));

  return device.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothGattService>>
LayoutTestBluetoothAdapterProvider::GetGattService(MockBluetoothDevice* device,
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
LayoutTestBluetoothAdapterProvider::GetGattCharacteristic(
    MockBluetoothGattService* service,
    const std::string& uuid) {
  return make_scoped_ptr(new NiceMock<MockBluetoothGattCharacteristic>(
      service, uuid /* identifier */, BluetoothUUID(uuid), false /* is_local */,
      NULL /* properties */, NULL /* permissions */));
}

}  // namespace content
