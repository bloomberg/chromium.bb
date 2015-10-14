// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattService;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothDiscoverySession;
using device::MockBluetoothGattCharacteristic;
using device::MockBluetoothGattConnection;
using device::MockBluetoothGattNotifySession;
using device::MockBluetoothGattService;
using testing::ElementsAre;
using testing::Invoke;
using testing::ResultOf;
using testing::Return;
using testing::_;

typedef testing::NiceMock<MockBluetoothAdapter> NiceMockBluetoothAdapter;
typedef testing::NiceMock<MockBluetoothDevice> NiceMockBluetoothDevice;
typedef testing::NiceMock<MockBluetoothDiscoverySession>
    NiceMockBluetoothDiscoverySession;
typedef testing::NiceMock<MockBluetoothGattCharacteristic>
    NiceMockBluetoothGattCharacteristic;
typedef testing::NiceMock<MockBluetoothGattConnection>
    NiceMockBluetoothGattConnection;
typedef testing::NiceMock<MockBluetoothGattService>
    NiceMockBluetoothGattService;
typedef testing::NiceMock<MockBluetoothGattNotifySession>
    NiceMockBluetoothGattNotifySession;

namespace {
// Bluetooth UUIDs suitable to pass to BluetoothUUID().
const char kBatteryServiceUUID[] = "180f";
const char kGenericAccessServiceUUID[] = "1800";
const char kGlucoseServiceUUID[] = "1808";
const char kHeartRateServiceUUID[] = "180d";
const char kHeartRateMeasurementUUID[] = "2a37";
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
  if (fake_adapter_name == "BaseAdapter")
    return GetBaseAdapter();
  else if (fake_adapter_name == "NotPresentAdapter")
    return GetNotPresentAdapter();
  else if (fake_adapter_name == "NotPoweredAdapter")
    return GetNotPoweredAdapter();
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
  else if (fake_adapter_name == "HeartRateAdapter")
    return GetHeartRateAdapter();
  else if (fake_adapter_name == "FailingConnectionsAdapter")
    return GetFailingConnectionsAdapter();
  else if (fake_adapter_name == "FailingGATTOperationsAdapter")
    return GetFailingGATTOperationsAdapter();
  else if (fake_adapter_name == "SecondDiscoveryFindsHeartRateAdapter")
    return GetSecondDiscoveryFindsHeartRateAdapter();
  else if (fake_adapter_name == "")
    return NULL;

  NOTREACHED() << fake_adapter_name;
  return NULL;
}

// Adapters

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBaseAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(
      new NiceMockBluetoothAdapter());

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
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPresentAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetBaseAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(true));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetNotPresentAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetBaseAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(false));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPoweredAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPresentAdapter());
  ON_CALL(*adapter, IsPowered()).WillByDefault(Return(true));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetNotPoweredAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPresentAdapter());
  ON_CALL(*adapter, IsPowered()).WillByDefault(Return(false));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetScanFilterCheckingAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

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
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailStartDiscoveryAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>());

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallbackWithResult<1 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetGlucoseHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));
  adapter->AddMockDevice(GetGlucoseDevice(adapter.get()));

  return adapter.Pass();
}

// Adds a device to |adapter| and notifies all observers about that new device.
// Mocks can call this asynchronously to cause changes in the middle of a test.
static void AddDevice(scoped_refptr<NiceMockBluetoothAdapter> adapter,
                      scoped_ptr<NiceMockBluetoothDevice> new_device) {
  NiceMockBluetoothDevice* new_device_ptr = new_device.get();
  adapter->AddMockDevice(new_device.Pass());
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, adapter->GetObservers(),
                    DeviceAdded(adapter.get(), new_device_ptr));
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetSecondDiscoveryFindsHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  EXPECT_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(RunCallbackWithResult<1 /* success_callback */>(
          []() { return GetDiscoverySession(); }))
      .WillOnce(
          RunCallbackWithResult<1 /* success_callback */>([adapter_ptr]() {
            // In the second discovery session, have the adapter discover a new
            // device, shortly after the session starts.
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::Bind(&AddDevice, make_scoped_refptr(adapter_ptr),

                           base::Passed(GetHeartRateDevice(adapter_ptr))));
            return GetDiscoverySession();
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetMissingServiceGenericAccessAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetGenericAccessDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter> LayoutTestBluetoothAdapterProvider::
    GetMissingCharacteristicGenericAccessAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  scoped_ptr<NiceMockBluetoothDevice> device(
      GetGenericAccessDevice(adapter.get()));

  scoped_ptr<NiceMockBluetoothGattService> generic_access(
      GetBaseGATTService(device.get(), kGenericAccessServiceUUID));

  // Intentionally NOT adding a characteristic to generic_access service.

  device->AddMockService(generic_access.Pass());

  adapter->AddMockDevice(device.Pass());

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetGenericAccessAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  scoped_ptr<NiceMockBluetoothDevice> device(
      GetGenericAccessDevice(adapter.get()));

  scoped_ptr<NiceMockBluetoothGattService> generic_access(
      GetBaseGATTService(device.get(), kGenericAccessServiceUUID));

  scoped_ptr<NiceMockBluetoothGattCharacteristic> device_name(
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

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  scoped_ptr<NiceMockBluetoothDevice> device(GetHeartRateDevice(adapter.get()));
  scoped_ptr<NiceMockBluetoothGattService> heart_rate(
      GetBaseGATTService(device.get(), kHeartRateServiceUUID));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975
  scoped_ptr<NiceMockBluetoothGattCharacteristic> heart_rate_measurement(
      GetBaseGATTCharacteristic(heart_rate.get(), kHeartRateMeasurementUUID));
  BluetoothGattCharacteristic* measurement_ptr = heart_rate_measurement.get();
  ON_CALL(*heart_rate_measurement, StartNotifySession(_, _))
      .WillByDefault(
          RunCallbackWithResult<0 /* success_callback */>([measurement_ptr]() {
            return GetBaseGATTNotifySession(measurement_ptr->GetIdentifier());
          }));

  heart_rate->AddMockCharacteristic(heart_rate_measurement.Pass());
  device->AddMockService(heart_rate.Pass());
  adapter->AddMockDevice(device.Pass());

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailingConnectionsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  for (int error = BluetoothDevice::ERROR_UNKNOWN;
       error <= BluetoothDevice::ERROR_UNSUPPORTED_DEVICE; error++) {
    adapter->AddMockDevice(GetUnconnectableDevice(
        adapter.get(), static_cast<BluetoothDevice::ConnectErrorCode>(error)));
  }

  return adapter.Pass();
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailingGATTOperationsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  const std::string errorsServiceUUID = errorUUID(0xA0);

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorsServiceUUID));

  scoped_ptr<NiceMockBluetoothDevice> device(
      GetConnectableDevice(adapter.get(), "Errors Device", uuids));

  scoped_ptr<NiceMockBluetoothGattService> service(
      GetBaseGATTService(device.get(), errorsServiceUUID));

  for (int error = BluetoothGattService::GATT_ERROR_UNKNOWN;
       error <= BluetoothGattService::GATT_ERROR_NOT_SUPPORTED; error++) {
    service->AddMockCharacteristic(GetErrorCharacteristic(
        service.get(),
        static_cast<BluetoothGattService::GattErrorCode>(error)));
  }

  device->AddMockService(service.Pass());
  adapter->AddMockDevice(device.Pass());

  return adapter.Pass();
}

// Discovery Sessions

// static
scoped_ptr<NiceMockBluetoothDiscoverySession>
LayoutTestBluetoothAdapterProvider::GetDiscoverySession() {
  scoped_ptr<NiceMockBluetoothDiscoverySession> discovery_session(
      new NiceMockBluetoothDiscoverySession());

  ON_CALL(*discovery_session, Stop(_, _))
      .WillByDefault(RunCallback<0 /* success_callback */>());

  return discovery_session.Pass();
}

// Devices

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetBaseDevice(
    MockBluetoothAdapter* adapter,
    const std::string& device_name,
    device::BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  scoped_ptr<NiceMockBluetoothDevice> device(new NiceMockBluetoothDevice(
      adapter, 0x1F00 /* Bluetooth class */, device_name, address,
      true /* paired */, true /* connected */));

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
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetBatteryDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kBatteryServiceUUID));

  return GetBaseDevice(adapter, "Battery Device", uuids, makeMACAddress(0x1));
}

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetGlucoseDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kGlucoseServiceUUID));

  return GetBaseDevice(adapter, "Glucose Device", uuids, makeMACAddress(0x2));
}

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetHeartRateDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));

  return GetConnectableDevice(adapter, "Heart Rate Device", uuids,
                              makeMACAddress(0x3));
}

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetConnectableDevice(
    device::MockBluetoothAdapter* adapter,
    const std::string& device_name,
    BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  scoped_ptr<NiceMockBluetoothDevice> device(
      GetBaseDevice(adapter, device_name, uuids, address));

  BluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, device_ptr]() {
            return make_scoped_ptr(new NiceMockBluetoothGattConnection(
                adapter, device_ptr->GetAddress()));
          }));

  return device.Pass();
}

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetUnconnectableDevice(
    MockBluetoothAdapter* adapter,
    BluetoothDevice::ConnectErrorCode error_code,
    const std::string& device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorUUID(error_code)));

  scoped_ptr<NiceMockBluetoothDevice> device(
      GetBaseDevice(adapter, device_name, uuids, makeMACAddress(error_code)));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  return device.Pass();
}

// static
scoped_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetGenericAccessDevice(
    MockBluetoothAdapter* adapter,
    const std::string& device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));

  return GetConnectableDevice(adapter, device_name, uuids);
}

// Services

// static
scoped_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetBaseGATTService(
    MockBluetoothDevice* device,
    const std::string& uuid) {
  scoped_ptr<NiceMockBluetoothGattService> service(
      new NiceMockBluetoothGattService(
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

// Characteristics

// static
scoped_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetBaseGATTCharacteristic(
    MockBluetoothGattService* service,
    const std::string& uuid) {
  return make_scoped_ptr(new NiceMockBluetoothGattCharacteristic(
      service, uuid + " Identifier", BluetoothUUID(uuid), false /* is_local */,
      NULL /* properties */, NULL /* permissions */));
}

// static
scoped_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetErrorCharacteristic(
    MockBluetoothGattService* service,
    BluetoothGattService::GattErrorCode error_code) {
  uint32_t error_alias = error_code + 0xA1;  // Error UUIDs start at 0xA1.
  scoped_ptr<NiceMockBluetoothGattCharacteristic> characteristic(
      GetBaseGATTCharacteristic(service, errorUUID(error_alias)));

  // Read response.
  ON_CALL(*characteristic, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  // Write response.
  ON_CALL(*characteristic, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>(error_code));

  // StartNotifySession response
  ON_CALL(*characteristic, StartNotifySession(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  return characteristic.Pass();
}

// Notify sessions

// static
scoped_ptr<NiceMockBluetoothGattNotifySession>
LayoutTestBluetoothAdapterProvider::GetBaseGATTNotifySession(
    const std::string& characteristic_identifier) {
  scoped_ptr<NiceMockBluetoothGattNotifySession> session(
      new NiceMockBluetoothGattNotifySession(characteristic_identifier));

  ON_CALL(*session, Stop(_)).WillByDefault(RunCallback<0>());

  return session.Pass();
}

// Helper functions

// static
std::string LayoutTestBluetoothAdapterProvider::errorUUID(uint32_t alias) {
  return base::StringPrintf("%08x-97e5-4cd7-b9f1-f5a427670c59", alias);
}

// static
std::string LayoutTestBluetoothAdapterProvider::makeMACAddress(uint64_t addr) {
  return BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012" PRIx64, addr));
}

}  // namespace content
