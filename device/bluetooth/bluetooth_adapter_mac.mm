// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter_mac_metrics.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

namespace {

// The frequency with which to poll the adapter for updates.
const int kPollIntervalMs = 500;

}  // namespace

namespace device {

CBCentralManagerState GetCBManagerState(CBCentralManager* manager) {
#if defined(MAC_OS_X_VERSION_10_13)
  return static_cast<CBCentralManagerState>([manager state]);
#else
  return [manager state];
#endif
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterMac::CreateAdapter();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapter() {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapterForTest(
    std::string name,
    std::string address,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->InitForTest(ui_task_runner);
  adapter->name_ = name;
  adapter->should_update_name_ = false;
  adapter->address_ = address;
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

// static
BluetoothUUID BluetoothAdapterMac::BluetoothUUIDWithCBUUID(CBUUID* uuid) {
  // UUIDString only available OS X >= 10.10.
  if (@available(macOS 10.10, *)) {
    std::string uuid_c_string = base::SysNSStringToUTF8([uuid UUIDString]);
    return device::BluetoothUUID(uuid_c_string);
  } else {
    DCHECK(false);
    return {};
  }
}

// static
std::string BluetoothAdapterMac::String(NSError* error) {
  if (!error) {
    return "no error";
  }
  return std::string("error domain: ") + base::SysNSStringToUTF8(error.domain) +
         ", code: " + std::to_string(error.code) + ", description: " +
         base::SysNSStringToUTF8(error.localizedDescription);
}

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      classic_powered_(false),
      num_discovery_sessions_(0),
      should_update_name_(true),
      classic_discovery_manager_(
          BluetoothDiscoveryManagerMac::CreateClassic(this)),
      weak_ptr_factory_(this) {
  if (IsLowEnergyAvailable()) {
    low_energy_discovery_manager_.reset(
        BluetoothLowEnergyDiscoveryManagerMac::Create(this));
    low_energy_central_manager_delegate_.reset(
        [[BluetoothLowEnergyCentralManagerDelegate alloc]
            initWithDiscoveryManager:low_energy_discovery_manager_.get()
                          andAdapter:this]);
    low_energy_central_manager_.reset([[CBCentralManager alloc]
        initWithDelegate:low_energy_central_manager_delegate_
                   queue:dispatch_get_main_queue()]);
    low_energy_discovery_manager_->SetCentralManager(
        low_energy_central_manager_);
  }
  DCHECK(classic_discovery_manager_);
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
  // When devices will be destroyed, they will need this current instance to
  // disconnect the gatt connection. To make sure they don't use the mac
  // adapter, they should be explicitly destroyed here.
  devices_.clear();
  // Set low_energy_central_manager_ to nil so no devices will try to use it
  // while being destroyed after this method. |devices_| is owned by
  // BluetoothAdapter.
  low_energy_central_manager_.reset();
}

std::string BluetoothAdapterMac::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterMac::GetName() const {
  if (!should_update_name_) {
    return name_;
  }

  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];
  if (controller == nil) {
    name_ = std::string();
  } else {
    name_ = base::SysNSStringToUTF8([controller nameAsString]);
    if (!name_.empty()) {
      should_update_name_ = false;
    }
  }
  return name_;
}

void BluetoothAdapterMac::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsInitialized() const {
  return true;
}

bool BluetoothAdapterMac::IsPresent() const {
  bool is_present = !address_.empty();
  if (IsLowEnergyAvailable()) {
    is_present =
        is_present || (GetCBManagerState(low_energy_central_manager_) !=
                       CBCentralManagerStateUnsupported);
  }
  return is_present;
}

bool BluetoothAdapterMac::IsPowered() const {
  bool is_powered = classic_powered_;
  if (IsLowEnergyAvailable()) {
    is_powered =
        is_powered || (GetCBManagerState(low_energy_central_manager_) ==
                       CBCentralManagerStatePoweredOn);
  }
  return is_powered;
}

void BluetoothAdapterMac::SetPowered(bool powered,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// TODO(krstnmnlsn): If this information is retrievable form IOBluetooth we
// should return the discoverable status.
bool BluetoothAdapterMac::IsDiscoverable() const {
  return false;
}

void BluetoothAdapterMac::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscovering() const {
  bool is_discovering = classic_discovery_manager_->IsDiscovering();
  if (IsLowEnergyAvailable())
    is_discovering =
        is_discovering || low_energy_discovery_manager_->IsDiscovering();
  return is_discovering;
}

std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
BluetoothAdapterMac::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const BluetoothDiscoveryFilter& discovery_filter) {
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
      connected_devices;
  std::set<device::BluetoothUUID> uuids;
  discovery_filter.GetUUIDs(uuids);
  if (uuids.empty()) {
    for (BluetoothDevice* device :
         RetrieveGattConnectedDevicesWithService(nullptr)) {
      connected_devices[device] = BluetoothDevice::UUIDSet();
    }
    return connected_devices;
  }
  for (const BluetoothUUID& uuid : uuids) {
    for (BluetoothDevice* device :
         RetrieveGattConnectedDevicesWithService(&uuid)) {
      connected_devices[device].insert(uuid);
    }
  }
  return connected_devices;
}

BluetoothAdapter::UUIDList BluetoothAdapterMac::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterMac::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingRfcomm(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingL2cap(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterMac::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

void BluetoothAdapterMac::ClassicDeviceFound(IOBluetoothDevice* device) {
  ClassicDeviceAdded(device);
}

void BluetoothAdapterMac::ClassicDiscoveryStopped(bool unexpected) {
  if (unexpected) {
    DVLOG(1) << "Discovery stopped unexpectedly";
    num_discovery_sessions_ = 0;
    MarkDiscoverySessionsAsInactive();
  }
  for (auto& observer : observers_)
    observer.AdapterDiscoveringChanged(this, false);
}

void BluetoothAdapterMac::DeviceConnected(IOBluetoothDevice* device) {
  // TODO(isherman): Investigate whether this method can be replaced with a call
  // to +registerForConnectNotifications:selector:.
  DVLOG(1) << "Adapter registered a new connection from device with address: "
           << BluetoothClassicDeviceMac::GetDeviceAddress(device);
  ClassicDeviceAdded(device);
}

// static
bool BluetoothAdapterMac::IsLowEnergyAvailable() {
  return base::mac::IsAtLeastOS10_10();
}

void BluetoothAdapterMac::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {}

void BluetoothAdapterMac::SetCentralManagerForTesting(
    CBCentralManager* central_manager) {
  CHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  central_manager.delegate = low_energy_central_manager_delegate_;
  low_energy_central_manager_.reset(central_manager,
                                    base::scoped_policy::RETAIN);
  low_energy_discovery_manager_->SetCentralManager(low_energy_central_manager_);
}

CBCentralManager* BluetoothAdapterMac::GetCentralManager() {
  return low_energy_central_manager_;
}

void BluetoothAdapterMac::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  DVLOG(1) << __func__;
  if (num_discovery_sessions_ > 0) {
    DCHECK(IsDiscovering());
    num_discovery_sessions_++;
    // We are already running a discovery session, notify the system if the
    // filter has changed.
    if (!StartDiscovery(discovery_filter)) {
      // TODO: Provide a more precise error here.
      error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      return;
    }
    callback.Run();
    return;
  }

  DCHECK_EQ(0, num_discovery_sessions_);

  if (!StartDiscovery(discovery_filter)) {
    // TODO: Provide a more precise error here.
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
    return;
  }

  DVLOG(1) << "Added a discovery session";
  num_discovery_sessions_++;
  for (auto& observer : observers_)
    observer.AdapterDiscoveringChanged(this, true);
  callback.Run();
}

void BluetoothAdapterMac::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  DVLOG(1) << __func__;

  if (num_discovery_sessions_ > 1) {
    // There are active sessions other than the one currently being removed.
    DCHECK(IsDiscovering());
    num_discovery_sessions_--;
    callback.Run();
    return;
  }

  if (num_discovery_sessions_ == 0) {
    DVLOG(1) << "No active discovery sessions. Returning error.";
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_ACTIVE);
    return;
  }

  // Default to dual discovery if |discovery_filter| is NULL.
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if (transport & BLUETOOTH_TRANSPORT_CLASSIC) {
    if (!classic_discovery_manager_->StopDiscovery()) {
      DVLOG(1) << "Failed to stop classic discovery";
      // TODO: Provide a more precise error here.
      error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      return;
    }
  }
  if (transport & BLUETOOTH_TRANSPORT_LE) {
    if (IsLowEnergyAvailable()) {
      low_energy_discovery_manager_->StopDiscovery();
      for (const auto& device_id_object_pair : devices_) {
        device_id_object_pair.second->ClearAdvertisementData();
      }
    }
  }

  DVLOG(1) << "Discovery stopped";
  num_discovery_sessions_--;
  callback.Run();
}

void BluetoothAdapterMac::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

bool BluetoothAdapterMac::StartDiscovery(
    BluetoothDiscoveryFilter* discovery_filter) {
  // Default to dual discovery if |discovery_filter| is NULL.  IOBluetooth seems
  // allow starting low energy and classic discovery at once.
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if ((transport & BLUETOOTH_TRANSPORT_CLASSIC) &&
      !classic_discovery_manager_->IsDiscovering()) {
    // TODO(krstnmnlsn): If a classic discovery session is already running then
    // we should update its filter. crbug.com/498056
    if (!classic_discovery_manager_->StartDiscovery()) {
      DVLOG(1) << "Failed to add a classic discovery session";
      return false;
    }
  }
  if (transport & BLUETOOTH_TRANSPORT_LE) {
    // Begin a low energy discovery session or update it if one is already
    // running.
    if (IsLowEnergyAvailable())
      low_energy_discovery_manager_->StartDiscovery(
          BluetoothDevice::UUIDList());
  }
  return true;
}

void BluetoothAdapterMac::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PollAdapter();
}

void BluetoothAdapterMac::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

void BluetoothAdapterMac::PollAdapter() {
  bool was_present = IsPresent();
  std::string address;
  bool classic_powered = false;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  if (controller != nil) {
    address = BluetoothDevice::CanonicalizeAddress(
        base::SysNSStringToUTF8([controller addressAsString]));
    classic_powered = ([controller powerState] == kBluetoothHCIPowerStateON);

    if (address != address_)
      should_update_name_ = true;
  }

  bool is_present = !address.empty();
  address_ = address;

  if (was_present != is_present) {
    for (auto& observer : observers_)
      observer.AdapterPresentChanged(this, is_present);
  }

  if (classic_powered_ != classic_powered) {
    classic_powered_ = classic_powered;
    for (auto& observer : observers_)
      observer.AdapterPoweredChanged(this, classic_powered_);
  }

  RemoveTimedOutDevices();
  AddPairedDevices();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterMac::PollAdapter,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::ClassicDeviceAdded(IOBluetoothDevice* device) {
  std::string device_address =
      BluetoothClassicDeviceMac::GetDeviceAddress(device);

  BluetoothDevice* device_classic = GetDevice(device_address);

  // Only notify observers once per device.
  if (device_classic != nullptr) {
    VLOG(3) << "Updating classic device: " << device_classic->GetAddress();
    device_classic->UpdateTimestamp();
    return;
  }

  device_classic = new BluetoothClassicDeviceMac(this, device);
  devices_[device_address] = base::WrapUnique(device_classic);
  VLOG(1) << "Adding new classic device: " << device_classic->GetAddress();

  for (auto& observer : observers_)
    observer.DeviceAdded(this, device_classic);
}

void BluetoothAdapterMac::LowEnergyDeviceUpdated(
    CBPeripheral* peripheral,
    NSDictionary* advertisement_data,
    int rssi) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  // If has no entry in the map, create new device and insert into |devices_|,
  // otherwise update the existing device.
  const bool is_new_device = device_mac == nullptr;
  if (is_new_device) {
    // A new device has been found.
    device_mac = new BluetoothLowEnergyDeviceMac(this, peripheral);
    VLOG(1) << *device_mac << ": New Device.";
  } else if (DoesCollideWithKnownDevice(peripheral, device_mac)) {
    return;
  }

  DCHECK(device_mac);
  VLOG(3) << *device_mac << ": Device updated with "
          << base::SysNSStringToUTF8([advertisement_data description]);

  // Get Advertised UUIDs
  // Core Specification Supplement (CSS) v7, Part 1.1
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdataserviceuuidskey
  BluetoothDevice::UUIDList advertised_uuids;
  NSArray* service_uuids =
      [advertisement_data objectForKey:CBAdvertisementDataServiceUUIDsKey];
  for (CBUUID* uuid in service_uuids) {
    advertised_uuids.push_back(
        BluetoothAdapterMac::BluetoothUUIDWithCBUUID(uuid));
  }
  NSArray* overflow_service_uuids = [advertisement_data
      objectForKey:CBAdvertisementDataOverflowServiceUUIDsKey];
  for (CBUUID* uuid in overflow_service_uuids) {
    advertised_uuids.push_back(
        BluetoothAdapterMac::BluetoothUUIDWithCBUUID(uuid));
  }

  // Get Service Data.
  // Core Specification Supplement (CSS) v7, Part 1.11
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdataservicedatakey
  BluetoothDevice::ServiceDataMap service_data_map;
  NSDictionary* service_data =
      [advertisement_data objectForKey:CBAdvertisementDataServiceDataKey];
  for (CBUUID* uuid in service_data) {
    NSData* data = [service_data objectForKey:uuid];
    const uint8_t* bytes = static_cast<const uint8_t*>([data bytes]);
    size_t length = [data length];
    service_data_map.emplace(BluetoothAdapterMac::BluetoothUUIDWithCBUUID(uuid),
                             std::vector<uint8_t>(bytes, bytes + length));
  }

  // Get Manufacturer Data.
  // "Size: 2 or more octets
  // The first 2 octets contain the Company Identifier Code followed
  // by additional manufacturer specific data"
  // Core Specification Supplement (CSS) v7, Part 1.4
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdatamanufacturerdatakey
  //
  BluetoothDevice::ManufacturerDataMap manufacturer_data_map;
  NSData* manufacturer_data =
      [advertisement_data objectForKey:CBAdvertisementDataManufacturerDataKey];
  const uint8_t* bytes = static_cast<const uint8_t*>([manufacturer_data bytes]);
  size_t length = [manufacturer_data length];
  if (length > 1) {
    const uint16_t manufacturer_id = bytes[0] | bytes[1] << 8;
    manufacturer_data_map.emplace(
        manufacturer_id, std::vector<uint8_t>(bytes + 2, bytes + length));
  }

  // Get Tx Power.
  // "Size: 1 octet
  //  0xXX: -127 to +127 dBm"
  // Core Specification Supplement (CSS) v7, Part 1.5
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdatatxpowerlevelkey
  NSNumber* tx_power =
      [advertisement_data objectForKey:CBAdvertisementDataTxPowerLevelKey];
  int8_t clamped_tx_power = BluetoothDevice::ClampPower([tx_power intValue]);

  device_mac->UpdateAdvertisementData(
      BluetoothDevice::ClampPower(rssi), std::move(advertised_uuids),
      std::move(service_data_map), std::move(manufacturer_data_map),
      tx_power == nil ? nullptr : &clamped_tx_power);

  if (is_new_device) {
    std::string device_address =
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
    devices_[device_address] = base::WrapUnique(device_mac);
    for (auto& observer : observers_)
      observer.DeviceAdded(this, device_mac);
  } else {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device_mac);
  }
}

// TODO(crbug.com/511025): Handle state < CBCentralManagerStatePoweredOff.
void BluetoothAdapterMac::LowEnergyCentralManagerUpdatedState() {
  VLOG(1) << "Central manager state updated: "
          << [low_energy_central_manager_ state];
  // A state with a value lower than CBCentralManagerStatePoweredOn implies that
  // scanning has stopped and that any connected peripherals have been
  // disconnected. Call DidDisconnectPeripheral manually to update the devices'
  // states since macOS doesn't call it.
  // See
  // https://developer.apple.com/reference/corebluetooth/cbcentralmanagerdelegate/1518888-centralmanagerdidupdatestate?language=objc
  if (GetCBManagerState(low_energy_central_manager_) <
      CBCentralManagerStatePoweredOn) {
    VLOG(1)
        << "Central no longer powered on. Notifying of device disconnection.";
    for (BluetoothDevice* device : GetDevices()) {
      BluetoothLowEnergyDeviceMac* device_mac =
          static_cast<BluetoothLowEnergyDeviceMac*>(device);
      if (device_mac->IsGattConnected()) {
        device_mac->DidDisconnectPeripheral(nullptr);
      }
    }
  }
}

void BluetoothAdapterMac::AddPairedDevices() {
  // Add any new paired devices.
  for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
    // pairedDevices sometimes includes unknown devices that are not paired.
    // Radar issue with id 2282763004 has been filed about it.
    if ([device isPaired]) {
      ClassicDeviceAdded(device);
    }
  }
}

std::vector<BluetoothDevice*>
BluetoothAdapterMac::RetrieveGattConnectedDevicesWithService(
    const BluetoothUUID* uuid) {
  NSArray* cbUUIDs = nil;
  if (!uuid) {
    VLOG(1) << "Retrieving all connected devices.";
    // It is not possible to ask for all connected peripherals with
    // -[CBCentralManager retrieveConnectedPeripheralsWithServices:] by passing
    // nil. To try to get most of the peripherals, the search is done with
    // Generic Access service.
    CBUUID* genericAccessServiceUUID = [CBUUID UUIDWithString:@"1800"];
    cbUUIDs = @[ genericAccessServiceUUID ];
  } else {
    VLOG(1) << "Retrieving connected devices with UUID: "
            << uuid->canonical_value();
    NSString* uuidString =
        base::SysUTF8ToNSString(uuid->canonical_value().c_str());
    cbUUIDs = @[ [CBUUID UUIDWithString:uuidString] ];
  }
  NSArray* peripherals = [low_energy_central_manager_
      retrieveConnectedPeripheralsWithServices:cbUUIDs];
  std::vector<BluetoothDevice*> connected_devices;
  for (CBPeripheral* peripheral in peripherals) {
    BluetoothLowEnergyDeviceMac* device_mac =
        GetBluetoothLowEnergyDeviceMac(peripheral);
    const bool is_new_device = device_mac == nullptr;

    if (!is_new_device && DoesCollideWithKnownDevice(peripheral, device_mac)) {
      continue;
    }
    if (is_new_device) {
      device_mac = new BluetoothLowEnergyDeviceMac(this, peripheral);
      std::string device_address =
          BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
      devices_[device_address] = base::WrapUnique(device_mac);
      for (auto& observer : observers_) {
        observer.DeviceAdded(this, device_mac);
      }
    }
    connected_devices.push_back(device_mac);
    VLOG(1) << *device_mac << ": New connected device.";
  }
  return connected_devices;
}

void BluetoothAdapterMac::CreateGattConnection(
    BluetoothLowEnergyDeviceMac* device_mac) {
  VLOG(1) << *device_mac << ": Create gatt connection.";
  [low_energy_central_manager_ connectPeripheral:device_mac->peripheral_
                                         options:nil];
}

void BluetoothAdapterMac::DisconnectGatt(
    BluetoothLowEnergyDeviceMac* device_mac) {
  VLOG(1) << *device_mac << ": Disconnect gatt.";
  [low_energy_central_manager_
      cancelPeripheralConnection:device_mac->peripheral_];
}

void BluetoothAdapterMac::DidConnectPeripheral(CBPeripheral* peripheral) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  device_mac->DidConnectPeripheral();
}

void BluetoothAdapterMac::DidFailToConnectPeripheral(CBPeripheral* peripheral,
                                                     NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  RecordDidFailToConnectPeripheralResult(error);
  BluetoothDevice::ConnectErrorCode error_code =
      BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN;
  if (error) {
    error_code = BluetoothDeviceMac::GetConnectErrorCodeFromNSError(error);
  }
  VLOG(1) << *device_mac << ": Failed to connect to peripheral with error "
          << BluetoothAdapterMac::String(error)
          << ", error code: " << error_code;
  device_mac->DidFailToConnectGatt(error_code);
}

void BluetoothAdapterMac::DidDisconnectPeripheral(CBPeripheral* peripheral,
                                                  NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  device_mac->DidDisconnectPeripheral(error);
}

BluetoothLowEnergyDeviceMac*
BluetoothAdapterMac::GetBluetoothLowEnergyDeviceMac(CBPeripheral* peripheral) {
  std::string device_address =
      BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  auto iter = devices_.find(device_address);
  if (iter == devices_.end()) {
    return nil;
  }
  return static_cast<BluetoothLowEnergyDeviceMac*>(iter->second.get());
}

bool BluetoothAdapterMac::DoesCollideWithKnownDevice(
    CBPeripheral* peripheral,
    BluetoothLowEnergyDeviceMac* device_mac) {
  // Check that there are no collisions.
  std::string stored_device_id = device_mac->GetIdentifier();
  std::string updated_device_id =
      BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(peripheral);
  if (stored_device_id != updated_device_id) {
    VLOG(1) << "LowEnergyDeviceUpdated stored_device_id != updated_device_id: "
            << std::endl
            << "  " << stored_device_id << std::endl
            << "  " << updated_device_id;
    // Collision, two identifiers map to the same hash address.  With a 48 bit
    // hash the probability of this occuring with 10,000 devices
    // simultaneously present is 1e-6 (see
    // https://en.wikipedia.org/wiki/Birthday_problem#Probability_table).  We
    // ignore the second device by returning.
    return true;
  }
  return false;
}

}  // namespace device
