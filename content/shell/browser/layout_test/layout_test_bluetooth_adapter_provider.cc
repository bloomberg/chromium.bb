// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattService;
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
using testing::InvokeWithoutArgs;
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
// Bluetooth UUIDs suitable to pass to BluetoothUUID():
// Services:
const char kBatteryServiceUUID[] = "180f";
const char kBlocklistTestServiceUUID[] = "611c954a-263b-4f4a-aab6-01ddb953f985";
const char kDeviceInformationServiceUUID[] = "180a";
const char kGenericAccessServiceUUID[] = "1800";
const char kGlucoseServiceUUID[] = "1808";
const char kHealthThermometerUUID[] = "1809";
const char kHeartRateServiceUUID[] = "180d";
const char kHumanInterfaceDeviceServiceUUID[] = "1812";
const char kRequestDisconnectionServiceUUID[] =
    "01d7d889-7451-419f-aeb8-d65e7b9277af";
const char kTxPowerServiceUUID[] = "1804";
// Characteristics:
const char kBlocklistExcludeReadsCharacteristicUUID[] =
    "bad1c9a2-9a5b-4015-8b60-1579bbbf2135";
const char kRequestDisconnectionCharacteristicUUID[] =
    "01d7d88a-7451-419f-aeb8-d65e7b9277af";
const char kBodySensorLocation[] = "2a38";
const char kDeviceNameUUID[] = "2a00";
const char kMeasurementIntervalUUID[] = "2a21";
const char kHeartRateMeasurementUUID[] = "2a37";
const char kSerialNumberStringUUID[] = "2a25";
const char kPeripheralPrivacyFlagUUID[] = "2a02";

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
}

// Notifies the adapter's observers for each device id the adapter.
void NotifyDevicesAdded(MockBluetoothAdapter* adapter) {
  for (BluetoothDevice* device : adapter->GetMockDevices()) {
    for (auto& observer : adapter->GetObservers())
      observer.DeviceAdded(adapter, device);
  }
}

// Notifies the adapter's observers that the services have been discovered.
void NotifyServicesDiscovered(MockBluetoothAdapter* adapter,
                              MockBluetoothDevice* device) {
  for (auto& observer : adapter->GetObservers())
    observer.GattServicesDiscovered(adapter, device);
}

// Notifies the adapter's observers that a device has changed.
void NotifyDeviceChanged(MockBluetoothAdapter* adapter,
                         MockBluetoothDevice* device) {
  for (auto& observer : adapter->GetObservers())
    observer.DeviceChanged(adapter, device);
}

void PerformReadValue(
    MockBluetoothAdapter* adapter,
    MockBluetoothGattCharacteristic* characteristic,
    const BluetoothRemoteGattCharacteristic::ValueCallback& callback,
    const std::vector<uint8_t>& value) {
  for (auto& observer : adapter->GetObservers()) {
    observer.GattCharacteristicValueChanged(adapter, characteristic, value);
  }
  callback.Run(value);
}

}  // namespace

namespace content {

// static
scoped_refptr<BluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(
    const std::string& fake_adapter_name) {
  if (fake_adapter_name == "BaseAdapter")
    return GetBaseAdapter();
  if (fake_adapter_name == "NotPresentAdapter")
    return GetNotPresentAdapter();
  if (fake_adapter_name == "NotPoweredAdapter")
    return GetNotPoweredAdapter();
  if (fake_adapter_name == "ScanFilterCheckingAdapter")
    return GetScanFilterCheckingAdapter();
  if (fake_adapter_name == "EmptyAdapter")
    return GetEmptyAdapter();
  if (fake_adapter_name == "FailStartDiscoveryAdapter")
    return GetFailStartDiscoveryAdapter();
  if (fake_adapter_name == "GlucoseHeartRateAdapter")
    return GetGlucoseHeartRateAdapter();
  if (fake_adapter_name == "UnicodeDeviceAdapter")
    return GetUnicodeDeviceAdapter();
  if (fake_adapter_name == "MissingServiceHeartRateAdapter")
    return GetMissingServiceHeartRateAdapter();
  if (fake_adapter_name == "MissingCharacteristicHeartRateAdapter")
    return GetMissingCharacteristicHeartRateAdapter();
  if (fake_adapter_name == "HeartRateAdapter")
    return GetHeartRateAdapter();
  if (fake_adapter_name == "EmptyNameHeartRateAdapter")
    return GetEmptyNameHeartRateAdapter();
  if (fake_adapter_name == "NoNameHeartRateAdapter")
    return GetNoNameHeartRateAdapter();
  if (fake_adapter_name == "TwoHeartRateServicesAdapter")
    return GetTwoHeartRateServicesAdapter();
  if (fake_adapter_name == "DisconnectingHeartRateAdapter")
    return GetDisconnectingHeartRateAdapter();
  if (fake_adapter_name == "DisconnectingHealthThermometerAdapter")
    return GetDisconnectingHealthThermometer();
  if (fake_adapter_name == "DisconnectingDuringServiceRetrievalAdapter")
    return GetServicesDiscoveredAfterReconnectionAdapter(true /* disconnect */);
  if (fake_adapter_name == "ServicesDiscoveredAfterReconnectionAdapter")
    return GetServicesDiscoveredAfterReconnectionAdapter(
        false /* disconnect */);
  if (fake_adapter_name == "DisconnectingDuringSuccessGATTOperationAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        true /* disconnect */, true /* succeeds */);
  }
  if (fake_adapter_name == "DisconnectingDuringFailureGATTOperationAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        true /* disconnect */, false /* succeeds */);
  }
  if (fake_adapter_name == "GATTOperationSucceedsAfterReconnectionAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        false /* disconnect */, true /* succeeds */);
  }
  if (fake_adapter_name == "GATTOperationFailsAfterReconnectionAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        false /* disconnect */, false /* succeeds */);
  }
  if (fake_adapter_name == "DisconnectingDuringStopNotifySessionAdapter") {
    return GetStopNotifySessionFinishesAfterReconnectionAdapter(
        true /* disconnect */);
  }
  if (fake_adapter_name ==
      "StopNotifySessionFinishesAfterReconnectionAdapter") {
    return GetStopNotifySessionFinishesAfterReconnectionAdapter(
        false /* disconnect */);
  }
  if (fake_adapter_name == "BlocklistTestAdapter")
    return GetBlocklistTestAdapter();
  if (fake_adapter_name == "FailingConnectionsAdapter")
    return GetFailingConnectionsAdapter();
  if (fake_adapter_name == "FailingGATTOperationsAdapter")
    return GetFailingGATTOperationsAdapter();
  if (fake_adapter_name == "SecondDiscoveryFindsHeartRateAdapter")
    return GetSecondDiscoveryFindsHeartRateAdapter();
  if (fake_adapter_name == "DeviceEventAdapter")
    return GetDeviceEventAdapter();
  if (fake_adapter_name == "DevicesRemovedAdapter")
    return GetDevicesRemovedAdapter();
  if (fake_adapter_name == "DelayedServicesDiscoveryAdapter")
    return GetDelayedServicesDiscoveryAdapter();
  if (fake_adapter_name.empty())
    return nullptr;
  // New adapters that can be used when fuzzing the Web Bluetooth API
  // should also be added to
  // src/third_party/WebKit/Source/modules/
  //   bluetooth/testing/clusterfuzz/constraints.py.

  NOTREACHED() << fake_adapter_name;
  return nullptr;
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

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPresentAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetBaseAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(true));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetNotPresentAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetBaseAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(false));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPoweredAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPresentAdapter());
  ON_CALL(*adapter, IsPowered()).WillByDefault(Return(true));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetNotPoweredAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPresentAdapter());
  ON_CALL(*adapter, IsPowered()).WillByDefault(Return(false));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetScanFilterCheckingAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  MockBluetoothAdapter* adapter_ptr = adapter.get();

  // This fails the test with an error message listing actual and expected UUIDs
  // if StartDiscoverySessionWithFilter() is called with the wrong argument.
  EXPECT_CALL(
      *adapter,
      StartDiscoverySessionWithFilterRaw(
          ResultOf(&GetUUIDs, ElementsAre(BluetoothUUID(kGlucoseServiceUUID),
                                          BluetoothUUID(kHeartRateServiceUUID),
                                          BluetoothUUID(kBatteryServiceUUID))),
          _, _))
      .WillRepeatedly(
          RunCallbackWithResult<1 /* success_callback */>([adapter_ptr]() {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::Bind(&NotifyDevicesAdded,
                                      base::RetainedRef(adapter_ptr)));

            return GetDiscoverySession();
          }));

  // Any unexpected call results in the failure callback.
  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>());

  // We need to add a device otherwise requestDevice would reject.
  adapter->AddMockDevice(GetBatteryDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailStartDiscoveryAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>());

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  MockBluetoothAdapter* adapter_ptr = adapter.get();

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(
          RunCallbackWithResult<1 /* success_callback */>([adapter_ptr]() {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::Bind(&NotifyDevicesAdded,
                                      base::RetainedRef(adapter_ptr)));

            return GetDiscoverySession();
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetGlucoseHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));
  adapter->AddMockDevice(GetGlucoseDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetUnicodeDeviceAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetBaseDevice(adapter.get(), "❤❤❤❤❤❤❤❤❤"));

  return adapter;
}

// Adds a device to |adapter| and notifies all observers about that new device.
// Mocks can call this asynchronously to cause changes in the middle of a test.
static void AddDevice(scoped_refptr<NiceMockBluetoothAdapter> adapter,
                      std::unique_ptr<NiceMockBluetoothDevice> new_device) {
  NiceMockBluetoothDevice* new_device_ptr = new_device.get();
  adapter->AddMockDevice(std::move(new_device));
  for (auto& observer : adapter->GetObservers())
    observer.DeviceAdded(adapter.get(), new_device_ptr);
}

static void RemoveDevice(scoped_refptr<NiceMockBluetoothAdapter> adapter,
                         const std::string& device_address) {
  std::unique_ptr<MockBluetoothDevice> removed_device =
      adapter->RemoveMockDevice(device_address);
  for (auto& observer : adapter->GetObservers())
    observer.DeviceRemoved(adapter.get(), removed_device.get());
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
LayoutTestBluetoothAdapterProvider::GetDeviceEventAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  // Add ConnectedHeartRateDevice.
  std::unique_ptr<NiceMockBluetoothDevice> connected_hr(GetBaseDevice(
      adapter.get(), "Connected Heart Rate Device",
      {BluetoothUUID(kHeartRateServiceUUID)}, makeMACAddress(0x0)));
  connected_hr->SetConnected(true);
  adapter->AddMockDevice(std::move(connected_hr));

  // Add ChangingBatteryDevice with no uuids.
  std::unique_ptr<NiceMockBluetoothDevice> changing_battery(
      GetBaseDevice(adapter.get(), "Changing Battery Device",
                    BluetoothDevice::UUIDList(), makeMACAddress(0x1)));
  changing_battery->SetConnected(false);

  NiceMockBluetoothDevice* changing_battery_ptr = changing_battery.get();
  adapter->AddMockDevice(std::move(changing_battery));

  // Add Non Connected Tx Power Device.
  std::unique_ptr<NiceMockBluetoothDevice> non_connected_tx_power(
      GetBaseDevice(adapter.get(), "Non Connected Tx Power Device",
                    {BluetoothUUID(kTxPowerServiceUUID)}, makeMACAddress(0x2)));
  non_connected_tx_power->SetConnected(false);
  adapter->AddMockDevice(std::move(non_connected_tx_power));

  // Add Discovery Generic Access Device with no uuids.
  std::unique_ptr<NiceMockBluetoothDevice> discovery_generic_access(
      GetBaseDevice(adapter.get(), "Discovery Generic Access Device",
                    BluetoothDevice::UUIDList(), makeMACAddress(0x3)));
  discovery_generic_access->SetConnected(true);

  NiceMockBluetoothDevice* discovery_generic_access_ptr =
      discovery_generic_access.get();
  adapter->AddMockDevice(std::move(discovery_generic_access));

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallbackWithResult<1 /* success_callback */>(
          [adapter_ptr, changing_battery_ptr, discovery_generic_access_ptr]() {
            if (adapter_ptr->GetDevices().size() == 4) {
              // Post task to add NewGlucoseDevice.
              std::unique_ptr<NiceMockBluetoothDevice> glucose_device(
                  GetBaseDevice(adapter_ptr, "New Glucose Device",
                                {BluetoothUUID(kGlucoseServiceUUID)},
                                makeMACAddress(0x4)));

              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  base::Bind(&AddDevice, make_scoped_refptr(adapter_ptr),
                             base::Passed(&glucose_device)));

              // Add uuid and notify of device changed.
              changing_battery_ptr->AddUUID(BluetoothUUID(kBatteryServiceUUID));
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, base::Bind(&NotifyDeviceChanged,
                                        base::RetainedRef(adapter_ptr),
                                        changing_battery_ptr));

              // Add uuid and notify of services discovered.
              discovery_generic_access_ptr->AddUUID(
                  BluetoothUUID(kGenericAccessServiceUUID));
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, base::Bind(&NotifyServicesDiscovered,
                                        base::RetainedRef(adapter_ptr),
                                        discovery_generic_access_ptr));
            }
            return GetDiscoverySession();
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetDevicesRemovedAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  // Add ConnectedHeartRateDevice.
  std::unique_ptr<NiceMockBluetoothDevice> connected_hr(GetBaseDevice(
      adapter.get(), "Connected Heart Rate Device",
      {BluetoothUUID(kHeartRateServiceUUID)}, makeMACAddress(0x0)));
  connected_hr->SetConnected(true);
  std::string connected_hr_address = connected_hr->GetAddress();
  adapter->AddMockDevice(std::move(connected_hr));

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallbackWithResult<1 /* success_callback */>(
          [adapter_ptr, connected_hr_address]() {
            if (adapter_ptr->GetDevices().size() == 1) {
              // Post task to add NewGlucoseDevice.
              std::unique_ptr<NiceMockBluetoothDevice> glucose_device(
                  GetBaseDevice(adapter_ptr, "New Glucose Device",
                                {BluetoothUUID(kGlucoseServiceUUID)},
                                makeMACAddress(0x4)));

              std::string glucose_address = glucose_device->GetAddress();

              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  base::Bind(&AddDevice, make_scoped_refptr(adapter_ptr),
                             base::Passed(&glucose_device)));

              // Post task to remove ConnectedHeartRateDevice.
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  base::Bind(&RemoveDevice, make_scoped_refptr(adapter_ptr),
                             connected_hr_address));

              // Post task to remove NewGlucoseDevice.
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  base::Bind(&RemoveDevice, make_scoped_refptr(adapter_ptr),
                             glucose_address));

            }
            return GetDiscoverySession();
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetMissingServiceHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetMissingCharacteristicHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  std::unique_ptr<NiceMockBluetoothGattService> generic_access(
      GetBaseGATTService("Generic Access", device.get(),
                         kGenericAccessServiceUUID));
  std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
      GetBaseGATTService("Heart Rate", device.get(), kHeartRateServiceUUID));

  // Intentionally NOT adding a characteristic to heart_rate service.

  device->AddMockService(std::move(generic_access));
  device->AddMockService(std::move(heart_rate));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetDelayedServicesDiscoveryAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  MockBluetoothAdapter* adapter_ptr = adapter.get();
  MockBluetoothDevice* device_ptr = device.get();

  // Override the previous mock implementation of
  // IsGattServicesDiscoveryComplete so that the first time the function is
  // called it returns false, adds a service and posts a task to notify
  // the services have been discovered. Subsequent calls to the function
  // will return true.
  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Invoke([adapter_ptr, device_ptr] {
        std::vector<BluetoothRemoteGattService*> services =
            device_ptr->GetMockServices();

        if (services.size() == 0) {
          std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
              GetBaseGATTService("Heart Rate", device_ptr,
                                 kHeartRateServiceUUID));

          device_ptr->AddMockService(std::move(heart_rate));
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&NotifyServicesDiscovered,
                         base::RetainedRef(adapter_ptr), device_ptr));

          DCHECK(services.size() == 0);
          return false;
        }

        return true;
      }));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetDisconnectingHealthThermometer() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  std::unique_ptr<NiceMockBluetoothDevice> device(GetConnectableDevice(
      adapter_ptr, "Disconnecting Health Thermometer",
      std::vector<BluetoothUUID>({BluetoothUUID(kGenericAccessServiceUUID),
                                  BluetoothUUID(kHealthThermometerUUID)})));

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  std::unique_ptr<NiceMockBluetoothGattService> health_thermometer(
      GetBaseGATTService("Health Thermometer", device.get(),
                         kHealthThermometerUUID));

  // Measurement Interval
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> measurement_interval(
      GetBaseGATTCharacteristic(
          "Measurement Interval", health_thermometer.get(),
          kMeasurementIntervalUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
              BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
              BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter_ptr, measurement_ptr]() {
            std::vector<uint8_t> interval({1});
            for (auto& observer : adapter_ptr->GetObservers()) {
              observer.GattCharacteristicValueChanged(
                  adapter_ptr, measurement_ptr, interval);
            }
            return interval;
          }));

  ON_CALL(*measurement_interval, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<1 /* success_callback */>());

  ON_CALL(*measurement_interval, StartNotifySession(_, _))
      .WillByDefault(
          RunCallbackWithResult<0 /* success_callback */>([measurement_ptr]() {
            return GetBaseGATTNotifySession(measurement_ptr->GetWeakPtr());
          }));

  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetEmptyNameHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get(), /* device_name */ ""));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetNoNameHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get(), /* device_name */ nullptr));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetTwoHeartRateServicesAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  device->AddMockService(GetGenericAccessService(device.get()));

  // First Heart Rate Service has one Heart Rate Measurement characteristic
  // and one Body Sensor Location characteristic.
  std::unique_ptr<NiceMockBluetoothGattService> first_heart_rate(
      GetBaseGATTService("First Heart Rate", device.get(),
                         kHeartRateServiceUUID));

  // Heart Rate Measurement
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> heart_rate_measurement(
      GetBaseGATTCharacteristic(
          "Heart Rate Measurement", first_heart_rate.get(),
          kHeartRateMeasurementUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));

  // Body Sensor Location Characteristic
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_chest(GetBaseGATTCharacteristic(
          "Body Sensor Location Chest", first_heart_rate.get(),
          kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  first_heart_rate->AddMockCharacteristic(std::move(heart_rate_measurement));
  first_heart_rate->AddMockCharacteristic(
      std::move(body_sensor_location_chest));
  device->AddMockService(std::move(first_heart_rate));

  // Second Heart Rate Service has only one Body Sensor Location
  // characteristic.
  std::unique_ptr<NiceMockBluetoothGattService> second_heart_rate(
      GetBaseGATTService("Second Heart Rate", device.get(),
                         kHeartRateServiceUUID));
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_wrist(GetBaseGATTCharacteristic(
          "Body Sensor Location Wrist", second_heart_rate.get(),
          kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  second_heart_rate->AddMockCharacteristic(
      std::move(body_sensor_location_wrist));
  device->AddMockService(std::move(second_heart_rate));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetDisconnectingHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter> LayoutTestBluetoothAdapterProvider::
    GetServicesDiscoveredAfterReconnectionAdapter(bool disconnect) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));
  NiceMockBluetoothDevice* device_ptr = device.get();

  // When called before IsGattDiscoveryComplete, run success callback with a new
  // Gatt connection. When called after after IsGattDiscoveryComplete runs
  // success callback with a new Gatt connection and notifies of services
  // discovered.
  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter_ptr, device_ptr]() {
            std::vector<BluetoothRemoteGattService*> services =
                device_ptr->GetMockServices();

            if (services.size() != 0) {
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  base::Bind(&NotifyServicesDiscovered,
                             base::RetainedRef(adapter_ptr), device_ptr));
            }

            return base::MakeUnique<NiceMockBluetoothGattConnection>(
                adapter_ptr, device_ptr->GetAddress());
          }));

  // The first time this function is called we:
  // 1. Add a service (This indicates that this function has been called)
  // 2. If |disconnect| is true, disconnect the device.
  // 3. Return false.
  // The second time this function is called we just return true.
  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Invoke([adapter_ptr, device_ptr, disconnect] {
        std::vector<BluetoothRemoteGattService*> services =
            device_ptr->GetMockServices();
        if (services.size() == 0) {
          std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
              GetBaseGATTService("Heart Rate", device_ptr,
                                 kHeartRateServiceUUID));

          device_ptr->AddMockService(GetGenericAccessService(device_ptr));
          device_ptr->AddMockService(
              GetHeartRateService(adapter_ptr, device_ptr));

          if (disconnect) {
            device_ptr->SetConnected(false);
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::Bind(&NotifyDeviceChanged, base::RetainedRef(adapter_ptr),
                           device_ptr));
          }
          DCHECK(services.size() == 0);
          return false;
        }

        return true;
      }));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter> LayoutTestBluetoothAdapterProvider::
    GetGATTOperationFinishesAfterReconnectionAdapter(bool disconnect,
                                                     bool succeeds) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  std::unique_ptr<NiceMockBluetoothDevice> device(GetConnectableDevice(
      adapter_ptr, "GATT Operation finishes after reconnection Device",
      BluetoothDevice::UUIDList({BluetoothUUID(kGenericAccessServiceUUID),
                                 BluetoothUUID(kHealthThermometerUUID)})));
  NiceMockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(Invoke([adapter_ptr, device_ptr](
          const BluetoothDevice::GattConnectionCallback& callback,
          const BluetoothDevice::ConnectErrorCallback& error_callback) {
        callback.Run(base::MakeUnique<NiceMockBluetoothGattConnection>(
            adapter_ptr, device_ptr->GetAddress()));
        device_ptr->RunPendingCallbacks();
      }));

  device->AddMockService(GetGenericAccessService(device.get()));

  std::unique_ptr<NiceMockBluetoothGattService> health_thermometer(
      GetBaseGATTService("Health Thermometer", device.get(),
                         kHealthThermometerUUID));

  // Measurement Interval
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> measurement_interval(
      GetBaseGATTCharacteristic(
          "Measurement Interval", health_thermometer.get(),
          kMeasurementIntervalUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
              BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
              BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, ReadRemoteCharacteristic(_, _))
      .WillByDefault(Invoke([adapter_ptr, device_ptr, measurement_ptr,
                             disconnect, succeeds](
          const BluetoothRemoteGattCharacteristic::ValueCallback& callback,
          const BluetoothRemoteGattCharacteristic::ErrorCallback&
              error_callback) {
        base::Closure pending;
        if (succeeds) {
          pending =
              base::Bind(&PerformReadValue, base::RetainedRef(adapter_ptr),
                         measurement_ptr, callback, std::vector<uint8_t>({1}));
        } else {
          pending = base::Bind(error_callback,
                               BluetoothRemoteGattService::GATT_ERROR_FAILED);
        }
        device_ptr->PushPendingCallback(pending);
        if (disconnect) {
          device_ptr->SetConnected(false);
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&NotifyDeviceChanged, base::RetainedRef(adapter_ptr),
                         device_ptr));
        }
      }));

  ON_CALL(*measurement_interval, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(Invoke([adapter_ptr, device_ptr, disconnect, succeeds](
          const std::vector<uint8_t>& value, const base::Closure& callback,
          const BluetoothRemoteGattCharacteristic::ErrorCallback&
              error_callback) {
        base::Closure pending;
        if (succeeds) {
          pending = callback;
        } else {
          pending = base::Bind(error_callback,
                               BluetoothRemoteGattService::GATT_ERROR_FAILED);
        }
        device_ptr->PushPendingCallback(pending);
        if (disconnect) {
          device_ptr->SetConnected(false);
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&NotifyDeviceChanged, base::RetainedRef(adapter_ptr),
                         device_ptr));
        }
      }));

  ON_CALL(*measurement_interval, StartNotifySession(_, _))
      .WillByDefault(Invoke([adapter_ptr, device_ptr, measurement_ptr,
                             disconnect, succeeds](
          const BluetoothRemoteGattCharacteristic::NotifySessionCallback&
              callback,
          const BluetoothRemoteGattCharacteristic::ErrorCallback&
              error_callback) {
        base::Closure pending;
        if (succeeds) {
          pending = base::Bind(callback, base::Passed(GetBaseGATTNotifySession(
                                             measurement_ptr->GetWeakPtr())));
        } else {
          pending = base::Bind(error_callback,
                               BluetoothRemoteGattService::GATT_ERROR_FAILED);
        }
        device_ptr->PushPendingCallback(pending);
        if (disconnect) {
          device_ptr->SetConnected(false);
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&NotifyDeviceChanged, base::RetainedRef(adapter_ptr),
                         device_ptr));
        }
      }));

  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));
  adapter->AddMockDevice(std::move(device));
  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter> LayoutTestBluetoothAdapterProvider::
    GetStopNotifySessionFinishesAfterReconnectionAdapter(bool disconnect) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  std::unique_ptr<NiceMockBluetoothDevice> device(GetConnectableDevice(
      adapter_ptr, "GATT Operation finishes after reconnection Device",
      BluetoothDevice::UUIDList({BluetoothUUID(kGenericAccessServiceUUID),
                                 BluetoothUUID(kHealthThermometerUUID)})));
  NiceMockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(Invoke([adapter_ptr, device_ptr](
          const BluetoothDevice::GattConnectionCallback& callback,
          const BluetoothDevice::ConnectErrorCallback& error_callback) {
        callback.Run(base::MakeUnique<NiceMockBluetoothGattConnection>(
            adapter_ptr, device_ptr->GetAddress()));
        device_ptr->RunPendingCallbacks();
      }));

  device->AddMockService(GetGenericAccessService(device.get()));

  std::unique_ptr<NiceMockBluetoothGattService> health_thermometer(
      GetBaseGATTService("Health Thermometer", device.get(),
                         kHealthThermometerUUID));

  // Measurement Interval
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> measurement_interval(
      GetBaseGATTCharacteristic(
          "Measurement Interval", health_thermometer.get(),
          kMeasurementIntervalUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, StartNotifySession(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter_ptr, device_ptr, measurement_ptr, disconnect]() {
            std::unique_ptr<NiceMockBluetoothGattNotifySession> notify_session =
                base::MakeUnique<NiceMockBluetoothGattNotifySession>(
                    measurement_ptr->GetWeakPtr());

            ON_CALL(*notify_session, Stop(_))
                .WillByDefault(Invoke([adapter_ptr, device_ptr, disconnect](
                    const base::Closure& callback) {

                  device_ptr->PushPendingCallback(callback);

                  if (disconnect) {
                    device_ptr->SetConnected(false);
                    base::ThreadTaskRunnerHandle::Get()->PostTask(
                        FROM_HERE,
                        base::Bind(&NotifyDeviceChanged,
                                   base::RetainedRef(adapter_ptr), device_ptr));
                  }
                }));
            return notify_session;
          }));

  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBlocklistTestAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kBlocklistTestServiceUUID));
  uuids.push_back(BluetoothUUID(kDeviceInformationServiceUUID));
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));
  uuids.push_back(BluetoothUUID(kHumanInterfaceDeviceServiceUUID));

  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetConnectableDevice(adapter.get(), "Blocklist Test Device", uuids));

  device->AddMockService(GetBlocklistTestService(device.get()));
  device->AddMockService(GetDeviceInformationService(device.get()));
  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  device->AddMockService(GetBaseGATTService("Human Interface Device",
                                            device.get(),
                                            kHumanInterfaceDeviceServiceUUID));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailingConnectionsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  for (int error = 0; error < BluetoothDevice::NUM_CONNECT_ERROR_CODES;
    error++) {
    adapter->AddMockDevice(GetUnconnectableDevice(
        adapter.get(), static_cast<BluetoothDevice::ConnectErrorCode>(error)));
  }

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetFailingGATTOperationsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  const std::string errorsServiceUUID = errorUUID(0xA0);

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorsServiceUUID));

  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetConnectableDevice(adapter.get(), "Errors Device", uuids));

  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  std::unique_ptr<NiceMockBluetoothGattService> service(
      GetBaseGATTService("Errors Service", device.get(), errorsServiceUUID));

  for (int error = BluetoothRemoteGattService::GATT_ERROR_UNKNOWN;
       error <= BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED; error++) {
    service->AddMockCharacteristic(GetErrorCharacteristic(
        service.get(),
        static_cast<BluetoothRemoteGattService::GattErrorCode>(error)));
  }

  device->AddMockService(std::move(service));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// Discovery Sessions

// static
std::unique_ptr<NiceMockBluetoothDiscoverySession>
LayoutTestBluetoothAdapterProvider::GetDiscoverySession() {
  std::unique_ptr<NiceMockBluetoothDiscoverySession> discovery_session(
      new NiceMockBluetoothDiscoverySession());

  ON_CALL(*discovery_session, Stop(_, _))
      .WillByDefault(RunCallback<0 /* success_callback */>());

  return discovery_session;
}

// Devices

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetBaseDevice(
    MockBluetoothAdapter* adapter,
    const char* device_name,
    device::BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  std::unique_ptr<NiceMockBluetoothDevice> device(new NiceMockBluetoothDevice(
      adapter, 0x1F00 /* Bluetooth class */, device_name, address,
      false /* paired */, false /* connected */));

  for (const auto& uuid : uuids) {
    device->AddUUID(uuid);
  }

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

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(
          BluetoothDevice::ERROR_UNSUPPORTED_DEVICE));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetBatteryDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kBatteryServiceUUID));

  return GetBaseDevice(adapter, "Battery Device", uuids, makeMACAddress(0x1));
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetGlucoseDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kGlucoseServiceUUID));
  uuids.push_back(BluetoothUUID(kTxPowerServiceUUID));

  return GetBaseDevice(adapter, "Glucose Device", uuids, makeMACAddress(0x2));
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetConnectableDevice(
    device::MockBluetoothAdapter* adapter,
    const char* device_name,
    BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetBaseDevice(adapter, device_name, uuids, address));

  MockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, device_ptr]() {
            return base::MakeUnique<NiceMockBluetoothGattConnection>(
                adapter, device_ptr->GetAddress());
          }));

  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Return(true));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetUnconnectableDevice(
    MockBluetoothAdapter* adapter,
    BluetoothDevice::ConnectErrorCode error_code,
    const char* device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorUUID(error_code)));

  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetBaseDevice(adapter, device_name, uuids, makeMACAddress(error_code)));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
LayoutTestBluetoothAdapterProvider::GetHeartRateDevice(
    MockBluetoothAdapter* adapter,
    const char* device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));

  return GetConnectableDevice(adapter, device_name, uuids);
}

// Services

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetBaseGATTService(
    const std::string& identifier,
    MockBluetoothDevice* device,
    const std::string& uuid) {
  std::unique_ptr<NiceMockBluetoothGattService> service(
      new NiceMockBluetoothGattService(device, identifier, BluetoothUUID(uuid),
                                       true /* is_primary */,
                                       false /* is_local */));

  ON_CALL(*service, GetCharacteristics())
      .WillByDefault(Invoke(service.get(),
                            &MockBluetoothGattService::GetMockCharacteristics));

  ON_CALL(*service, GetCharacteristic(_))
      .WillByDefault(Invoke(service.get(),
                            &MockBluetoothGattService::GetMockCharacteristic));

  return service;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetBlocklistTestService(
    device::MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> blocklist_test_service(
      GetBaseGATTService("Blocklist Test", device, kBlocklistTestServiceUUID));

  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      blocklist_exclude_reads_characteristic(GetBaseGATTCharacteristic(
          "Excluded Reads Characteristic", blocklist_test_service.get(),
          kBlocklistExcludeReadsCharacteristicUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
              BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in layout tests that may not report a mock expectation
  // error correctly as a layout test failure.
  ON_CALL(*blocklist_exclude_reads_characteristic,
          ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          Invoke([](const BluetoothRemoteGattCharacteristic::ValueCallback&,
                    const BluetoothRemoteGattCharacteristic::ErrorCallback&) {
            NOTREACHED();
          }));

  // Write response.
  ON_CALL(*blocklist_exclude_reads_characteristic,
          WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<1 /* success callback */>());

  blocklist_test_service->AddMockCharacteristic(
      std::move(blocklist_exclude_reads_characteristic));

  return blocklist_test_service;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetDeviceInformationService(
    device::MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> device_information(
      GetBaseGATTService("Device Information", device,
                         kDeviceInformationServiceUUID));

  std::unique_ptr<NiceMockBluetoothGattCharacteristic> serial_number_string(
      GetBaseGATTCharacteristic(
          "Serial Number String", device_information.get(),
          kSerialNumberStringUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in layout tests that may not report a mock expectation
  // error correctly as a layout test failure.
  ON_CALL(*serial_number_string, ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          Invoke([](const BluetoothRemoteGattCharacteristic::ValueCallback&,
                    const BluetoothRemoteGattCharacteristic::ErrorCallback&) {
            NOTREACHED();
          }));

  device_information->AddMockCharacteristic(std::move(serial_number_string));

  return device_information;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetGenericAccessService(
    device::MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> generic_access(
      GetBaseGATTService("Generic Access", device, kGenericAccessServiceUUID));

  {  // Device Name:
    std::unique_ptr<NiceMockBluetoothGattCharacteristic> device_name(
        GetBaseGATTCharacteristic(
            "Device Name", generic_access.get(), kDeviceNameUUID,
            BluetoothRemoteGattCharacteristic::PROPERTY_READ |
                BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

    // Read response.
    std::vector<uint8_t> device_name_value;
    if (base::Optional<std::string> name = device->GetName())
      device_name_value.assign(name.value().begin(), name.value().end());
    ON_CALL(*device_name, ReadRemoteCharacteristic(_, _))
        .WillByDefault(RunCallback<0>(device_name_value));

    // Write response.
    ON_CALL(*device_name, WriteRemoteCharacteristic(_, _, _))
        .WillByDefault(RunCallback<1 /* success callback */>());

    generic_access->AddMockCharacteristic(std::move(device_name));
  }

  {  // Peripheral Privacy Flag:
    std::unique_ptr<NiceMockBluetoothGattCharacteristic>
        peripheral_privacy_flag(GetBaseGATTCharacteristic(
            "Peripheral Privacy Flag", generic_access.get(),
            kPeripheralPrivacyFlagUUID,
            BluetoothRemoteGattCharacteristic::PROPERTY_READ |
                BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

    // Read response.
    std::vector<uint8_t> value(1);
    value[0] = false;

    ON_CALL(*peripheral_privacy_flag, ReadRemoteCharacteristic(_, _))
        .WillByDefault(RunCallback<0>(value));

    // Crash if WriteRemoteCharacteristic called. Not using GoogleMock's Expect
    // because this is used in layout tests that may not report a mock
    // expectation error correctly as a layout test failure.
    ON_CALL(*peripheral_privacy_flag, WriteRemoteCharacteristic(_, _, _))
        .WillByDefault(
            Invoke([](const std::vector<uint8_t>&, const base::Closure&,
                      const BluetoothRemoteGattCharacteristic::ErrorCallback&) {
              NOTREACHED();
            }));

    generic_access->AddMockCharacteristic(std::move(peripheral_privacy_flag));
  }

  return generic_access;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetHeartRateService(
    MockBluetoothAdapter* adapter,
    MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
      GetBaseGATTService("Heart Rate", device, kHeartRateServiceUUID));

  // Heart Rate Measurement
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> heart_rate_measurement(
      GetBaseGATTCharacteristic(
          "Heart Rate Measurement", heart_rate.get(), kHeartRateMeasurementUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      heart_rate_measurement.get();

  ON_CALL(*heart_rate_measurement, StartNotifySession(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, measurement_ptr]() {
            std::unique_ptr<NiceMockBluetoothGattNotifySession> notify_session(
                GetBaseGATTNotifySession(measurement_ptr->GetWeakPtr()));

            std::vector<uint8_t> rate(1 /* size */);
            rate[0] = 60;

            notify_session->StartTestNotifications(adapter, measurement_ptr,
                                                   rate);

            return notify_session;
          }));

  // Body Sensor Location Characteristic (Chest)
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_chest(GetBaseGATTCharacteristic(
          "Body Sensor Location Chest", heart_rate.get(), kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));
  BluetoothRemoteGattCharacteristic* location_chest_ptr =
      body_sensor_location_chest.get();

  ON_CALL(*body_sensor_location_chest, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, location_chest_ptr]() {
            std::vector<uint8_t> location(1 /* size */);
            location[0] = 1;  // Chest
            // Read a characteristic has a side effect of
            // GattCharacteristicValueChanged being called.
            for (auto& observer : adapter->GetObservers()) {
              observer.GattCharacteristicValueChanged(
                  adapter, location_chest_ptr, location);
            }
            return location;
          }));

  // Body Sensor Location Characteristic (Wrist)
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_wrist(GetBaseGATTCharacteristic(
          "Body Sensor Location Wrist", heart_rate.get(), kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));
  BluetoothRemoteGattCharacteristic* location_wrist_ptr =
      body_sensor_location_wrist.get();

  ON_CALL(*body_sensor_location_wrist, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, location_wrist_ptr]() {
            std::vector<uint8_t> location(1 /* size */);
            location[0] = 2;  // Wrist
            // Read a characteristic has a side effect of
            // GattCharacteristicValueChanged being called.
            for (auto& observer : adapter->GetObservers()) {
              observer.GattCharacteristicValueChanged(
                  adapter, location_wrist_ptr, location);
            }
            return location;
          }));

  heart_rate->AddMockCharacteristic(std::move(heart_rate_measurement));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_chest));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_wrist));

  return heart_rate;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetDisconnectingService(
    MockBluetoothAdapter* adapter,
    MockBluetoothDevice* device) {
  // Set up a service and a characteristic to disconnect the device when it's
  // written to.
  std::unique_ptr<NiceMockBluetoothGattService> disconnection_service =
      GetBaseGATTService("Disconnection", device,
                         kRequestDisconnectionServiceUUID);
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      disconnection_characteristic(GetBaseGATTCharacteristic(
          "Disconnection Characteristic", disconnection_service.get(),
          kRequestDisconnectionCharacteristicUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));
  ON_CALL(*disconnection_characteristic, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(Invoke([adapter, device](
          const std::vector<uint8_t>& value, const base::Closure& success,
          const BluetoothRemoteGattCharacteristic::ErrorCallback& error) {
        device->SetConnected(false);
        for (auto& observer : adapter->GetObservers())
          observer.DeviceChanged(adapter, device);
        success.Run();
      }));

  disconnection_service->AddMockCharacteristic(
      std::move(disconnection_characteristic));
  return disconnection_service;
}

// Characteristics

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetBaseGATTCharacteristic(
    const std::string& identifier,
    MockBluetoothGattService* service,
    const std::string& uuid,
    BluetoothRemoteGattCharacteristic::Properties properties) {
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> characteristic(
      new NiceMockBluetoothGattCharacteristic(
          service, identifier, BluetoothUUID(uuid), false /* is_local */,
          properties, NULL /* permissions */));

  // Read response.
  ON_CALL(*characteristic, ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          RunCallback<1>(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));

  // Write response.
  ON_CALL(*characteristic, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(
          RunCallback<2>(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));

  // StartNotifySession response
  ON_CALL(*characteristic, StartNotifySession(_, _))
      .WillByDefault(
          RunCallback<1>(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));

  return characteristic;
}

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetErrorCharacteristic(
    MockBluetoothGattService* service,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  uint32_t error_alias = error_code + 0xA1;  // Error UUIDs start at 0xA1.
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> characteristic(
      GetBaseGATTCharacteristic(
          // Use the UUID to generate unique identifiers.
          "Error Characteristic " + errorUUID(error_alias), service,
          errorUUID(error_alias),
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
              BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
              BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE));

  // Read response.
  ON_CALL(*characteristic, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  // Write response.
  ON_CALL(*characteristic, WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>(error_code));

  // StartNotifySession response
  ON_CALL(*characteristic, StartNotifySession(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  return characteristic;
}

// Notify sessions

// static
std::unique_ptr<NiceMockBluetoothGattNotifySession>
LayoutTestBluetoothAdapterProvider::GetBaseGATTNotifySession(
    base::WeakPtr<device::BluetoothRemoteGattCharacteristic> characteristic) {
  std::unique_ptr<NiceMockBluetoothGattNotifySession> session(
      new NiceMockBluetoothGattNotifySession(characteristic));

  ON_CALL(*session, Stop(_))
      .WillByDefault(testing::DoAll(
          InvokeWithoutArgs(
              session.get(),
              &MockBluetoothGattNotifySession::StopTestNotifications),
          RunCallback<0>()));

  return session;
}

// Helper functions

// static
std::string LayoutTestBluetoothAdapterProvider::errorUUID(uint32_t alias) {
  return base::StringPrintf("%08x-97e5-4cd7-b9f1-f5a427670c59", alias);
}

// static
BluetoothUUID LayoutTestBluetoothAdapterProvider::connectErrorUUID(
    BluetoothDevice::ConnectErrorCode error_code) {
  // Case values listed in alphabetical order.
  // Associated UUIDs are defined in layout tests and should remain stable
  // even if BluetoothDevice enum values change.
  switch (error_code) {
    case BluetoothDevice::ERROR_ATTRIBUTE_LENGTH_INVALID:
      return BluetoothUUID("0008");
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      return BluetoothUUID("0004");
    case BluetoothDevice::ERROR_AUTH_FAILED:
      return BluetoothUUID("0003");
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      return BluetoothUUID("0005");
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return BluetoothUUID("0006");
    case BluetoothDevice::ERROR_CONNECTION_CONGESTED:
      return BluetoothUUID("0009");
    case BluetoothDevice::ERROR_FAILED:
      return BluetoothUUID("0002");
    case BluetoothDevice::ERROR_INPROGRESS:
      return BluetoothUUID("0001");
    case BluetoothDevice::ERROR_INSUFFICIENT_ENCRYPTION:
      return BluetoothUUID("000a");
    case BluetoothDevice::ERROR_OFFSET_INVALID:
      return BluetoothUUID("000b");
    case BluetoothDevice::ERROR_READ_NOT_PERMITTED:
      return BluetoothUUID("000c");
    case BluetoothDevice::ERROR_REQUEST_NOT_SUPPORTED:
      return BluetoothUUID("000d");
    case BluetoothDevice::ERROR_UNKNOWN:
      return BluetoothUUID("0000");
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return BluetoothUUID("0007");
    case BluetoothDevice::ERROR_WRITE_NOT_PERMITTED:
      return BluetoothUUID("000e");
    case BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      return BluetoothUUID();
  }
  NOTREACHED();
  return BluetoothUUID();
}

// static
std::string LayoutTestBluetoothAdapterProvider::makeMACAddress(uint64_t addr) {
  return BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012" PRIx64, addr));
}

}  // namespace content
