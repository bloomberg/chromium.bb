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

using base::StringPiece;
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
const char kBlacklistTestServiceUUID[] = "611c954a-263b-4f4a-aab6-01ddb953f985";
const char kDeviceInformationServiceUUID[] = "180a";
const char kGenericAccessServiceUUID[] = "1800";
const char kGlucoseServiceUUID[] = "1808";
const char kHeartRateServiceUUID[] = "180d";
const char kHumanInterfaceDeviceServiceUUID[] = "1812";
const char kTxPowerServiceUUID[] = "1804";
// Characteristics:
const char kBlacklistExcludeReadsCharacteristicUUID[] =
    "bad1c9a2-9a5b-4015-8b60-1579bbbf2135";
const char kBodySensorLocation[] = "2a38";
const char kDeviceNameUUID[] = "2a00";
const char kHeartRateMeasurementUUID[] = "2a37";
const char kSerialNumberStringUUID[] = "2a25";
const char kPeripheralPrivacyFlagUUID[] = "2a02";

const int kDefaultTxPower = -10;  // TxPower of a device broadcasting at 0.1mW.
const int kDefaultRssi = -51;     // RSSI at 1m from a device broadcasting at
                                  // 0.1mW.

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

// Notifies the adapter's observers that the services have been discovered.
void NotifyServicesDiscovered(MockBluetoothAdapter* adapter,
                              MockBluetoothDevice* device) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, adapter->GetObservers(),
                    GattServicesDiscovered(adapter, device));
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
  if (fake_adapter_name == "BlacklistTestAdapter")
    return GetBlacklistTestAdapter();
  if (fake_adapter_name == "FailingConnectionsAdapter")
    return GetFailingConnectionsAdapter();
  if (fake_adapter_name == "FailingGATTOperationsAdapter")
    return GetFailingGATTOperationsAdapter();
  if (fake_adapter_name == "SecondDiscoveryFindsHeartRateAdapter")
    return GetSecondDiscoveryFindsHeartRateAdapter();
  if (fake_adapter_name == "DelayedServicesDiscoveryAdapter")
    return GetDelayedServicesDiscoveryAdapter();
  if (fake_adapter_name.empty())
    return nullptr;

  if (base::StartsWith(fake_adapter_name, "PowerValueAdapter",
                       base::CompareCase::SENSITIVE)) {
    std::vector<StringPiece> args =
        base::SplitStringPiece(fake_adapter_name, ":", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    DCHECK(args[0] == "PowerValueAdapter");
    DCHECK(args.size() == 3) << "Should contain AdapterName:TxPower:RSSI";

    int tx_power;
    base::StringToInt(args[1], &tx_power);
    DCHECK(tx_power >= INT8_MIN && tx_power <= INT8_MAX)
        << "Must be between [-128, 127]";
    int rssi;
    base::StringToInt(args[2], &rssi);
    DCHECK(rssi >= INT8_MIN && rssi <= INT8_MAX)
        << "Must be between [-128, 127]";
    return GetPowerValueAdapter(tx_power, rssi);
  }

  if (base::StartsWith(fake_adapter_name, "PowerPresenceAdapter",
                       base::CompareCase::SENSITIVE)) {
    std::vector<StringPiece> args =
        base::SplitStringPiece(fake_adapter_name, ":", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    DCHECK(args[0] == "PowerPresenceAdapter");
    DCHECK(args.size() == 3)
        << "Should contain AdapterName:TxPowerPresent:RSSIPResent";
    DCHECK(args[1] == "true" || args[1] == "false");
    DCHECK(args[2] == "true" || args[2] == "false");

    return GetPowerPresenceAdapter(args[1] == "true" /* tx_power_present */,
                                   args[2] == "true" /* rssi_present */);
  }

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

  ON_CALL(*adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillByDefault(RunCallbackWithResult<1 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPowerValueAdapter(int8_t tx_power,
                                                         int8_t rssi) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetHeartRateDevice(adapter.get()));

  ON_CALL(*device, GetInquiryTxPower()).WillByDefault(Return(tx_power));
  ON_CALL(*device, GetInquiryRSSI()).WillByDefault(Return(rssi));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetPowerPresenceAdapter(
    bool tx_power_present,
    bool rssi_present) {
  // TODO(ortuno): RSSI Unknown and Tx Power Unknown should have different
  // values. Add kUnknownTxPower when implemented: http://crbug.com/551572
  int tx_power =
      tx_power_present ? kDefaultTxPower : BluetoothDevice::kUnknownPower;
  int rssi = rssi_present ? kDefaultRssi : BluetoothDevice::kUnknownPower;

  return GetPowerValueAdapter(tx_power, rssi);
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
      GetBaseGATTService(device.get(), kGenericAccessServiceUUID));
  std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
      GetBaseGATTService(device.get(), kHeartRateServiceUUID));

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
        std::vector<BluetoothGattService*> services =
            device_ptr->GetMockServices();

        if (services.size() == 0) {
          std::unique_ptr<NiceMockBluetoothGattService> heart_rate(
              GetBaseGATTService(device_ptr, kHeartRateServiceUUID));

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
LayoutTestBluetoothAdapterProvider::GetBlacklistTestAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kBlacklistTestServiceUUID));
  uuids.push_back(BluetoothUUID(kDeviceInformationServiceUUID));
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));
  uuids.push_back(BluetoothUUID(kHumanInterfaceDeviceServiceUUID));

  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetConnectableDevice(adapter.get(), "Blacklist Test Device", uuids));

  device->AddMockService(GetBlacklistTestService(device.get()));
  device->AddMockService(GetDeviceInformationService(device.get()));
  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  device->AddMockService(
      GetBaseGATTService(device.get(), kHumanInterfaceDeviceServiceUUID));
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

  std::unique_ptr<NiceMockBluetoothGattService> service(
      GetBaseGATTService(device.get(), errorsServiceUUID));

  for (int error = BluetoothGattService::GATT_ERROR_UNKNOWN;
       error <= BluetoothGattService::GATT_ERROR_NOT_SUPPORTED; error++) {
    service->AddMockCharacteristic(GetErrorCharacteristic(
        service.get(),
        static_cast<BluetoothGattService::GattErrorCode>(error)));
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
    const std::string& device_name,
    device::BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  std::unique_ptr<NiceMockBluetoothDevice> device(new NiceMockBluetoothDevice(
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
    const std::string& device_name,
    BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  std::unique_ptr<NiceMockBluetoothDevice> device(
      GetBaseDevice(adapter, device_name, uuids, address));

  MockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, device_ptr]() {
            return base::WrapUnique(new NiceMockBluetoothGattConnection(
                adapter, device_ptr->GetAddress()));
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
    const std::string& device_name) {
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
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));

  return GetConnectableDevice(adapter, "Heart Rate Device", uuids);
}

// Services

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetBaseGATTService(
    MockBluetoothDevice* device,
    const std::string& uuid) {
  std::unique_ptr<NiceMockBluetoothGattService> service(
      new NiceMockBluetoothGattService(
          device, uuid /* identifier */, BluetoothUUID(uuid),
          true /* is_primary */, false /* is_local */));

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
LayoutTestBluetoothAdapterProvider::GetBlacklistTestService(
    device::MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> blacklist_test_service(
      GetBaseGATTService(device, kBlacklistTestServiceUUID));

  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      blacklist_exclude_reads_characteristic(GetBaseGATTCharacteristic(
          "Excluded Reads Characteristic", blacklist_test_service.get(),
          kBlacklistExcludeReadsCharacteristicUUID,
          BluetoothGattCharacteristic::PROPERTY_READ |
              BluetoothGattCharacteristic::PROPERTY_WRITE));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in layout tests that may not report a mock expectation
  // error correctly as a layout test failure.
  ON_CALL(*blacklist_exclude_reads_characteristic,
          ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          Invoke([](const BluetoothGattCharacteristic::ValueCallback&,
                    const BluetoothGattCharacteristic::ErrorCallback&) {
            NOTREACHED();
          }));

  // Write response.
  ON_CALL(*blacklist_exclude_reads_characteristic,
          WriteRemoteCharacteristic(_, _, _))
      .WillByDefault(RunCallback<1 /* success callback */>());

  blacklist_test_service->AddMockCharacteristic(
      std::move(blacklist_exclude_reads_characteristic));

  return blacklist_test_service;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
LayoutTestBluetoothAdapterProvider::GetDeviceInformationService(
    device::MockBluetoothDevice* device) {
  std::unique_ptr<NiceMockBluetoothGattService> device_information(
      GetBaseGATTService(device, kDeviceInformationServiceUUID));

  std::unique_ptr<NiceMockBluetoothGattCharacteristic> serial_number_string(
      GetBaseGATTCharacteristic(
          "Serial Number String", device_information.get(),
          kSerialNumberStringUUID, BluetoothGattCharacteristic::PROPERTY_READ));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in layout tests that may not report a mock expectation
  // error correctly as a layout test failure.
  ON_CALL(*serial_number_string, ReadRemoteCharacteristic(_, _))
      .WillByDefault(
          Invoke([](const BluetoothGattCharacteristic::ValueCallback&,
                    const BluetoothGattCharacteristic::ErrorCallback&) {
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
      GetBaseGATTService(device, kGenericAccessServiceUUID));

  {  // Device Name:
    std::unique_ptr<NiceMockBluetoothGattCharacteristic> device_name(
        GetBaseGATTCharacteristic(
            "Device Name", generic_access.get(), kDeviceNameUUID,
            BluetoothGattCharacteristic::PROPERTY_READ |
                BluetoothGattCharacteristic::PROPERTY_WRITE));

    // Read response.
    std::string device_name_str = device->GetDeviceName();
    std::vector<uint8_t> device_name_value(device_name_str.begin(),
                                           device_name_str.end());

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
            BluetoothGattCharacteristic::PROPERTY_READ |
                BluetoothGattCharacteristic::PROPERTY_WRITE));

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
                      const BluetoothGattCharacteristic::ErrorCallback&) {
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
      GetBaseGATTService(device, kHeartRateServiceUUID));

  // Heart Rate Measurement
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> heart_rate_measurement(
      GetBaseGATTCharacteristic("Heart Rate Measurement", heart_rate.get(),
                                kHeartRateMeasurementUUID,
                                BluetoothGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      heart_rate_measurement.get();

  ON_CALL(*heart_rate_measurement, StartNotifySession(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, measurement_ptr]() {
            std::unique_ptr<NiceMockBluetoothGattNotifySession> notify_session(
                GetBaseGATTNotifySession(measurement_ptr->GetIdentifier()));

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
          BluetoothGattCharacteristic::PROPERTY_READ));
  BluetoothGattCharacteristic* location_chest_ptr =
      body_sensor_location_chest.get();

  ON_CALL(*body_sensor_location_chest, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, location_chest_ptr]() {
            std::vector<uint8_t> location(1 /* size */);
            location[0] = 1;  // Chest
            // Read a characteristic has a side effect of
            // GattCharacteristicValueChanged being called.
            FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                              adapter->GetObservers(),
                              GattCharacteristicValueChanged(
                                  adapter, location_chest_ptr, location));
            return location;
          }));

  // Body Sensor Location Characteristic (Wrist)
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_wrist(GetBaseGATTCharacteristic(
          "Body Sensor Location Wrist", heart_rate.get(), kBodySensorLocation,
          BluetoothGattCharacteristic::PROPERTY_READ));
  BluetoothGattCharacteristic* location_wrist_ptr =
      body_sensor_location_wrist.get();

  ON_CALL(*body_sensor_location_wrist, ReadRemoteCharacteristic(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, location_wrist_ptr]() {
            std::vector<uint8_t> location(1 /* size */);
            location[0] = 2;  // Wrist
            // Read a characteristic has a side effect of
            // GattCharacteristicValueChanged being called.
            FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                              adapter->GetObservers(),
                              GattCharacteristicValueChanged(
                                  adapter, location_wrist_ptr, location));
            return location;
          }));

  heart_rate->AddMockCharacteristic(std::move(heart_rate_measurement));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_chest));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_wrist));

  return heart_rate;
}

// Characteristics

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetBaseGATTCharacteristic(
    const std::string& identifier,
    MockBluetoothGattService* service,
    const std::string& uuid,
    BluetoothGattCharacteristic::Properties properties) {
  return base::WrapUnique(new NiceMockBluetoothGattCharacteristic(
      service, identifier, BluetoothUUID(uuid), false /* is_local */,
      properties, NULL /* permissions */));
}

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
LayoutTestBluetoothAdapterProvider::GetErrorCharacteristic(
    MockBluetoothGattService* service,
    BluetoothGattService::GattErrorCode error_code) {
  uint32_t error_alias = error_code + 0xA1;  // Error UUIDs start at 0xA1.
  std::unique_ptr<NiceMockBluetoothGattCharacteristic> characteristic(
      GetBaseGATTCharacteristic(
          // Use the UUID to generate unique identifiers.
          "Error Characteristic " + errorUUID(error_alias), service,
          errorUUID(error_alias),
          BluetoothGattCharacteristic::PROPERTY_READ |
              BluetoothGattCharacteristic::PROPERTY_WRITE |
              BluetoothGattCharacteristic::PROPERTY_INDICATE));

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
    const std::string& characteristic_identifier) {
  std::unique_ptr<NiceMockBluetoothGattNotifySession> session(
      new NiceMockBluetoothGattNotifySession(characteristic_identifier));

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
