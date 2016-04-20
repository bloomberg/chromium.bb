// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/arc_bluetooth_bridge.h"

#include <fcntl.h>
#include <stddef.h>

#include <iomanip>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/bluetooth/bluetooth_type_converters.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDiscoverySession;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;

namespace arc {

ArcBluetoothBridge::ArcBluetoothBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_factory_(this) {
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    VLOG(1) << "registering bluetooth adapter";
    BluetoothAdapterFactory::GetAdapter(base::Bind(
        &ArcBluetoothBridge::OnAdapterInitialized, weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "no bluetooth adapter available";
  }

  arc_bridge_service()->AddObserver(this);
}

ArcBluetoothBridge::~ArcBluetoothBridge() {
  arc_bridge_service()->RemoveObserver(this);

  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void ArcBluetoothBridge::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void ArcBluetoothBridge::OnBluetoothInstanceReady() {
  mojom::BluetoothInstance* bluetooth_instance =
      arc_bridge_service()->bluetooth_instance();
  if (!bluetooth_instance) {
    LOG(ERROR) << "OnBluetoothInstanceReady called, "
               << "but no bluetooth instance found";
    return;
  }

  arc_bridge_service()->bluetooth_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcBluetoothBridge::AdapterPresentChanged(BluetoothAdapter* adapter,
                                               bool present) {
  // If the adapter goes away, remove ourselves as an observer.
  if (!present && adapter == bluetooth_adapter_) {
    adapter->RemoveObserver(this);
    bluetooth_adapter_ = nullptr;
  }
}

void ArcBluetoothBridge::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Invoke EnableAdapter or DisableAdapter via ARC bridge
  // service.
}

void ArcBluetoothBridge::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!HasBluetoothInstance())
    return;

  if (device->IsConnected())
    return;

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device);

  arc_bridge_service()->bluetooth_instance()->OnDeviceFound(
      std::move(properties));
}

void ArcBluetoothBridge::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  // TODO(smbarber): device properties changed; inform the container.
}

void ArcBluetoothBridge::DeviceAddressChanged(BluetoothAdapter* adapter,
                                              BluetoothDevice* device,
                                              const std::string& old_address) {
  // Must be implemented for LE Privacy/GATT client.
}

void ArcBluetoothBridge::DevicePairedChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             bool new_paired_status) {
  DCHECK(adapter);
  DCHECK(device);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  if (new_paired_status) {
    // OnBondStateChanged must be called with BluetoothBondState::BONDING to
    // make sure the bond state machine on Android is ready to take the
    // pair-done event. Otherwise the pair-done event will be dropped as an
    // invalid change of paired status.
    OnPairing(addr->Clone());
    OnPairedDone(std::move(addr));
  } else {
    OnForgetDone(std::move(addr));
  }
}

void ArcBluetoothBridge::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  DCHECK(adapter);
  DCHECK(device);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());
  OnForgetDone(std::move(addr));
}

void ArcBluetoothBridge::GattServiceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device,
                                          BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServicesDiscovered(BluetoothAdapter* adapter,
                                                BluetoothDevice* device) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::EnableAdapter(const EnableAdapterCallback& callback) {
  DCHECK(bluetooth_adapter_);
  if (!bluetooth_adapter_->IsPowered()) {
    bluetooth_adapter_->SetPowered(
        true, base::Bind(&ArcBluetoothBridge::OnPoweredOn,
                         weak_factory_.GetWeakPtr(), callback),
        base::Bind(&ArcBluetoothBridge::OnPoweredError,
                   weak_factory_.GetWeakPtr(), callback));
    return;
  }

  OnPoweredOn(callback);
}

void ArcBluetoothBridge::DisableAdapter(
    const DisableAdapterCallback& callback) {
  DCHECK(bluetooth_adapter_);
  bluetooth_adapter_->SetPowered(
      false, base::Bind(&ArcBluetoothBridge::OnPoweredOff,
                        weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ArcBluetoothBridge::OnPoweredError,
                 weak_factory_.GetWeakPtr(), callback));
}

void ArcBluetoothBridge::GetAdapterProperty(mojom::BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetAdapterProperties(type);

  arc_bridge_service()->bluetooth_instance()->OnAdapterProperties(
      mojom::BluetoothStatus::SUCCESS, std::move(properties));
}

void ArcBluetoothBridge::SetAdapterProperty(
    mojom::BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Implement SetAdapterProperty
  arc_bridge_service()->bluetooth_instance()->OnAdapterProperties(
      mojom::BluetoothStatus::FAIL,
      mojo::Array<mojom::BluetoothPropertyPtr>::New(0));
}

void ArcBluetoothBridge::GetRemoteDeviceProperty(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  std::string addr_str = remote_addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetDeviceProperties(type, device);
  mojom::BluetoothStatus status = mojom::BluetoothStatus::SUCCESS;

  if (!device) {
    VLOG(1) << __func__ << ": device " << addr_str << " not available";
    status = mojom::BluetoothStatus::FAIL;
  }

  arc_bridge_service()->bluetooth_instance()->OnRemoteDeviceProperties(
      status, std::move(remote_addr), std::move(properties));
}

void ArcBluetoothBridge::SetRemoteDeviceProperty(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Implement SetRemoteDeviceProperty
  arc_bridge_service()->bluetooth_instance()->OnRemoteDeviceProperties(
      mojom::BluetoothStatus::FAIL, std::move(remote_addr),
      mojo::Array<mojom::BluetoothPropertyPtr>::New(0));
}

void ArcBluetoothBridge::GetRemoteServiceRecord(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothUUIDPtr uuid) {
  // TODO(smbarber): Implement GetRemoteServiceRecord
}

void ArcBluetoothBridge::GetRemoteServices(
    mojom::BluetoothAddressPtr remote_addr) {
  // TODO(smbarber): Implement GetRemoteServices
}

void ArcBluetoothBridge::StartDiscovery() {
  DCHECK(bluetooth_adapter_);
  // TODO(smbarber): Add timeout
  if (discovery_session_) {
    LOG(ERROR) << "Discovery session already running; leaving alone";
    SendCachedDevicesFound();
    return;
  }

  bluetooth_adapter_->StartDiscoverySession(
      base::Bind(&ArcBluetoothBridge::OnDiscoveryStarted,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ArcBluetoothBridge::OnDiscoveryError,
                 weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::CancelDiscovery() {
  if (!discovery_session_) {
    return;
  }

  discovery_session_->Stop(base::Bind(&ArcBluetoothBridge::OnDiscoveryStopped,
                                      weak_factory_.GetWeakPtr()),
                           base::Bind(&ArcBluetoothBridge::OnDiscoveryError,
                                      weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::OnPoweredOn(
    const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  callback.Run(mojom::BluetoothAdapterState::ON);
  SendCachedPairedDevices();
}

void ArcBluetoothBridge::OnPoweredOff(
    const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  callback.Run(mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnPoweredError(
    const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  LOG(WARNING) << "failed to change power state";

  callback.Run(bluetooth_adapter_->IsPowered()
                   ? mojom::BluetoothAdapterState::ON
                   : mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnDiscoveryStarted(
    scoped_ptr<BluetoothDiscoverySession> session) {
  if (!HasBluetoothInstance())
    return;

  discovery_session_ = std::move(session);

  arc_bridge_service()->bluetooth_instance()->OnDiscoveryStateChanged(
      mojom::BluetoothDiscoveryState::STARTED);

  SendCachedDevicesFound();
}

void ArcBluetoothBridge::OnDiscoveryStopped() {
  if (!HasBluetoothInstance())
    return;

  discovery_session_.reset();

  arc_bridge_service()->bluetooth_instance()->OnDiscoveryStateChanged(
      mojom::BluetoothDiscoveryState::STOPPED);
}

void ArcBluetoothBridge::CreateBond(mojom::BluetoothAddressPtr addr,
                                    int32_t transport) {
  std::string addr_str = addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
  if (!device || !device->IsPairable()) {
    VLOG(1) << __func__ << ": device " << addr_str
            << " is no longer valid or pairable";
    OnPairedError(std::move(addr), BluetoothDevice::ERROR_FAILED);
    return;
  }

  if (device->IsPaired()) {
    OnPairedDone(std::move(addr));
    return;
  }

  // Use the default pairing delegate which is the delegate registered and owned
  // by ash.
  BluetoothDevice::PairingDelegate* delegate =
      bluetooth_adapter_->DefaultPairingDelegate();

  if (!delegate) {
    OnPairedError(std::move(addr), BluetoothDevice::ERROR_FAILED);
    return;
  }

  // If pairing finished successfully, DevicePairedChanged will notify Android
  // on paired state change event, so DoNothing is passed as a success callback.
  device->Pair(delegate, base::Bind(&base::DoNothing),
               base::Bind(&ArcBluetoothBridge::OnPairedError,
                          weak_factory_.GetWeakPtr(), base::Passed(&addr)));
}

void ArcBluetoothBridge::RemoveBond(mojom::BluetoothAddressPtr addr) {
  // Forget the device if it is no longer valid or not even paired.
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device || !device->IsPaired()) {
    OnForgetDone(std::move(addr));
    return;
  }

  // If unpairing finished successfully, DevicePairedChanged will notify Android
  // on paired state change event, so DoNothing is passed as a success callback.
  device->Forget(base::Bind(&base::DoNothing),
                 base::Bind(&ArcBluetoothBridge::OnForgetError,
                            weak_factory_.GetWeakPtr(), base::Passed(&addr)));
}

void ArcBluetoothBridge::CancelBond(mojom::BluetoothAddressPtr addr) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    OnForgetDone(std::move(addr));
    return;
  }

  device->CancelPairing();
  OnForgetDone(std::move(addr));
}

void ArcBluetoothBridge::GetConnectionState(
    mojom::BluetoothAddressPtr addr,
    const GetConnectionStateCallback& callback) {
  if (!bluetooth_adapter_) {
    callback.Run(false);
    return;
  }

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    callback.Run(false);
    return;
  }

  callback.Run(device->IsConnected());
}

void ArcBluetoothBridge::OnDiscoveryError() {
  LOG(WARNING) << "failed to change discovery state";
}

void ArcBluetoothBridge::OnPairing(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth_instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::BONDING);
}

void ArcBluetoothBridge::OnPairedDone(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth_instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::BONDED);
}

void ArcBluetoothBridge::OnPairedError(
    mojom::BluetoothAddressPtr addr,
    BluetoothDevice::ConnectErrorCode error_code) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth_instance()->OnBondStateChanged(
      mojom::BluetoothStatus::FAIL, std::move(addr),
      mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetDone(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth_instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetError(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  mojom::BluetoothBondState bond_state = mojom::BluetoothBondState::NONE;
  if (device && device->IsPaired()) {
    bond_state = mojom::BluetoothBondState::BONDED;
  }
  arc_bridge_service()->bluetooth_instance()->OnBondStateChanged(
      mojom::BluetoothStatus::FAIL, std::move(addr), bond_state);
}

mojo::Array<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetDeviceProperties(mojom::BluetoothPropertyType type,
                                        BluetoothDevice* device) const {
  mojo::Array<mojom::BluetoothPropertyPtr> properties;

  if (!device) {
    return properties;
  }

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdname(mojo::String::From(base::UTF16ToUTF8(device->GetName())));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdaddr(mojom::BluetoothAddress::From(device->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    std::vector<device::BluetoothUUID> uuids = device->GetUUIDs();
    mojo::Array<mojom::BluetoothUUIDPtr> uuid_results =
        mojo::Array<mojom::BluetoothUUIDPtr>::New(0);

    for (size_t i = 0; i < uuids.size(); i++) {
      uuid_results.push_back(mojom::BluetoothUUID::From(uuids[i]));
    }

    btp->set_uuids(std::move(uuid_results));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_class(device->GetBluetoothClass());
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    // TODO(smbarber): This needs to be populated with the actual device type
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_type(mojom::BluetoothDeviceType::DUAL);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_FRIENDLY_NAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdname(mojo::String::From(base::UTF16ToUTF8(device->GetName())));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_RSSI) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_remote_rssi(device->GetInquiryRSSI());
    properties.push_back(std::move(btp));
  }
  // TODO(smbarber): Add remote version info

  return properties;
}

mojo::Array<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetAdapterProperties(
    mojom::BluetoothPropertyType type) const {
  mojo::Array<mojom::BluetoothPropertyPtr> properties;

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    std::string name = bluetooth_adapter_->GetName();
    btp->set_bdname(mojo::String(name));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdaddr(
        mojom::BluetoothAddress::From(bluetooth_adapter_->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    // TODO(smbarber): Fill in once GetUUIDs is available for the adapter.
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual adapter class
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_class(0);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual device type
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_type(mojom::BluetoothDeviceType::DUAL);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_SCAN_MODE) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    mojom::BluetoothScanMode scan_mode = mojom::BluetoothScanMode::CONNECTABLE;

    if (bluetooth_adapter_->IsDiscoverable())
      scan_mode = mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE;

    btp->set_adapter_scan_mode(scan_mode);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_BONDED_DEVICES) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();

    mojo::Array<mojom::BluetoothAddressPtr> bonded_devices =
        mojo::Array<mojom::BluetoothAddressPtr>::New(0);

    for (size_t i = 0; i < devices.size(); i++) {
      if (!devices[i]->IsPaired())
        continue;

      mojom::BluetoothAddressPtr addr =
          mojom::BluetoothAddress::From(devices[i]->GetAddress());
      bonded_devices.push_back(std::move(addr));
    }

    btp->set_bonded_devices(std::move(bonded_devices));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_DISCOVERY_TIMEOUT) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_discovery_timeout(120);
    properties.push_back(std::move(btp));
  }

  return properties;
}

void ArcBluetoothBridge::SendCachedDevicesFound() const {
  // Send devices that have already been discovered, but aren't connected.
  if (!HasBluetoothInstance())
    return;

  BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
  for (size_t i = 0; i < devices.size(); i++) {
    if (devices[i]->IsPaired())
      continue;

    mojo::Array<mojom::BluetoothPropertyPtr> properties =
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, devices[i]);

    arc_bridge_service()->bluetooth_instance()->OnDeviceFound(
        std::move(properties));
  }
}

bool ArcBluetoothBridge::HasBluetoothInstance() const {
  if (!arc_bridge_service()->bluetooth_instance()) {
    LOG(WARNING) << "no Bluetooth instance available";
    return false;
  }

  return true;
}

void ArcBluetoothBridge::SendCachedPairedDevices() const {
  DCHECK(bluetooth_adapter_);

  BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
  for (BluetoothDevice* device : devices) {
    if (!device->IsPaired())
      continue;

    mojo::Array<mojom::BluetoothPropertyPtr> properties =
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device);

    arc_bridge_service()->bluetooth_instance()->OnDeviceFound(
        std::move(properties));

    mojom::BluetoothAddressPtr addr =
        mojom::BluetoothAddress::From(device->GetAddress());

    // OnBondStateChanged must be called with mojom::BluetoothBondState::BONDING
    // to
    // make sure the bond state machine on Android is ready to take the
    // pair-done event. Otherwise the pair-done event will be dropped as an
    // invalid change of paired status.
    OnPairing(addr->Clone());
    OnPairedDone(std::move(addr));
  }
}

}  // namespace arc
