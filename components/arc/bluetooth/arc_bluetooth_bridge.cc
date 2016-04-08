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
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDiscoverySession;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;

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
  BluetoothInstance* bluetooth_instance =
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

  mojo::Array<BluetoothPropertyPtr> properties =
      GetDeviceProperties(BluetoothPropertyType::ALL, device);

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

void ArcBluetoothBridge::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  // TODO(smbarber): device removed; inform the container.
}

void ArcBluetoothBridge::GattServiceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device,
                                          BluetoothGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceRemoved(BluetoothAdapter* adapter,
                                            BluetoothDevice* device,
                                            BluetoothGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServicesDiscovered(BluetoothAdapter* adapter,
                                                BluetoothDevice* device) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceChanged(BluetoothAdapter* adapter,
                                            BluetoothGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::EnableAdapter(const EnableAdapterCallback& callback) {
  DCHECK(bluetooth_adapter_);
  bluetooth_adapter_->SetPowered(
      true, base::Bind(&ArcBluetoothBridge::OnPoweredOn,
                       weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ArcBluetoothBridge::OnPoweredError,
                 weak_factory_.GetWeakPtr(), callback));
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

void ArcBluetoothBridge::GetAdapterProperty(BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  mojo::Array<BluetoothPropertyPtr> properties = GetAdapterProperties(type);

  arc_bridge_service()->bluetooth_instance()->OnAdapterProperties(
      BluetoothStatus::SUCCESS, std::move(properties));
}

void ArcBluetoothBridge::SetAdapterProperty(BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Implement SetAdapterProperty
  arc_bridge_service()->bluetooth_instance()->OnAdapterProperties(
      BluetoothStatus::FAIL, mojo::Array<BluetoothPropertyPtr>::New(0));
}

void ArcBluetoothBridge::GetRemoteDeviceProperty(
    BluetoothAddressPtr remote_addr,
    BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  std::string addr_str = remote_addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);

  mojo::Array<BluetoothPropertyPtr> properties =
      GetDeviceProperties(type, device);

  arc_bridge_service()->bluetooth_instance()->OnRemoteDeviceProperties(
      BluetoothStatus::SUCCESS, std::move(remote_addr), std::move(properties));
}

void ArcBluetoothBridge::SetRemoteDeviceProperty(
    BluetoothAddressPtr remote_addr,
    BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Implement SetRemoteDeviceProperty
  arc_bridge_service()->bluetooth_instance()->OnRemoteDeviceProperties(
      BluetoothStatus::FAIL, std::move(remote_addr),
      mojo::Array<BluetoothPropertyPtr>::New(0));
}

void ArcBluetoothBridge::GetRemoteServiceRecord(BluetoothAddressPtr remote_addr,
                                                BluetoothUUIDPtr uuid) {
  // TODO(smbarber): Implement GetRemoteServiceRecord
}

void ArcBluetoothBridge::GetRemoteServices(BluetoothAddressPtr remote_addr) {
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
    const mojo::Callback<void(BluetoothAdapterState)>& callback) const {
  callback.Run(BluetoothAdapterState::ON);
}

void ArcBluetoothBridge::OnPoweredOff(
    const mojo::Callback<void(BluetoothAdapterState)>& callback) const {
  callback.Run(BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnPoweredError(
    const mojo::Callback<void(BluetoothAdapterState)>& callback) const {
  LOG(WARNING) << "failed to change power state";

  callback.Run(bluetooth_adapter_->IsPowered() ? BluetoothAdapterState::ON
                                               : BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnDiscoveryStarted(
    scoped_ptr<BluetoothDiscoverySession> session) {
  if (!HasBluetoothInstance())
    return;

  discovery_session_ = std::move(session);

  arc_bridge_service()->bluetooth_instance()->OnDiscoveryStateChanged(
      BluetoothDiscoveryState::STARTED);

  SendCachedDevicesFound();
}

void ArcBluetoothBridge::OnDiscoveryStopped() {
  if (!HasBluetoothInstance())
    return;

  discovery_session_.reset();

  arc_bridge_service()->bluetooth_instance()->OnDiscoveryStateChanged(
      BluetoothDiscoveryState::STOPPED);
}

void ArcBluetoothBridge::CreateBond(BluetoothAddressPtr addr,
                                    int32_t transport) {
  // TODO(smbarber): Implement CreateBond
}

void ArcBluetoothBridge::RemoveBond(BluetoothAddressPtr addr) {
  // TODO(smbarber): Implement RemoveBond
}

void ArcBluetoothBridge::CancelBond(BluetoothAddressPtr addr) {
  // TODO(smbarber): Implement CancelBond
}

void ArcBluetoothBridge::GetConnectionState(
    BluetoothAddressPtr addr,
    const GetConnectionStateCallback& callback) {
  if (!bluetooth_adapter_) {
    callback.Run(false);
    return;
  }

  std::string addr_str = addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
  if (!device) {
    callback.Run(false);
    return;
  }

  callback.Run(device->IsConnected());
}

void ArcBluetoothBridge::OnDiscoveryError() {
  LOG(WARNING) << "failed to change discovery state";
}

mojo::Array<BluetoothPropertyPtr> ArcBluetoothBridge::GetDeviceProperties(
    BluetoothPropertyType type,
    BluetoothDevice* device) {
  mojo::Array<BluetoothPropertyPtr> properties;

  if (!device) {
    return properties;
  }

  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::BDNAME) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_bdname(mojo::String::From(base::UTF16ToUTF8(device->GetName())));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::BDADDR) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_bdaddr(BluetoothAddress::From(device->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::UUIDS) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    std::vector<device::BluetoothUUID> uuids = device->GetUUIDs();
    mojo::Array<BluetoothUUIDPtr> uuid_results =
        mojo::Array<BluetoothUUIDPtr>::New(0);

    for (size_t i = 0; i < uuids.size(); i++) {
      uuid_results.push_back(BluetoothUUID::From(uuids[i]));
    }

    btp->set_uuids(std::move(uuid_results));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::CLASS_OF_DEVICE) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_device_class(device->GetBluetoothClass());
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::TYPE_OF_DEVICE) {
    // TODO(smbarber): This needs to be populated with the actual device type
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_device_type(BluetoothDeviceType::DUAL);
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::REMOTE_FRIENDLY_NAME) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_bdname(mojo::String::From(base::UTF16ToUTF8(device->GetName())));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::REMOTE_RSSI) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_remote_rssi(device->GetInquiryRSSI());
    properties.push_back(std::move(btp));
  }
  // TODO(smbarber): Add remote version info

  return properties;
}

mojo::Array<BluetoothPropertyPtr> ArcBluetoothBridge::GetAdapterProperties(
    BluetoothPropertyType type) {
  mojo::Array<BluetoothPropertyPtr> properties;

  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::BDNAME) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    std::string name = bluetooth_adapter_->GetName();
    btp->set_bdname(mojo::String(name));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::BDADDR) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_bdaddr(BluetoothAddress::From(bluetooth_adapter_->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::UUIDS) {
    // TODO(smbarber): Fill in once GetUUIDs is available for the adapter.
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::CLASS_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual adapter class
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_device_class(0);
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::TYPE_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual device type
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_device_type(BluetoothDeviceType::DUAL);
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::ADAPTER_SCAN_MODE) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    BluetoothScanMode scan_mode = BluetoothScanMode::CONNECTABLE;

    if (bluetooth_adapter_->IsDiscoverable())
      scan_mode = BluetoothScanMode::CONNECTABLE_DISCOVERABLE;

    btp->set_adapter_scan_mode(scan_mode);
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::ADAPTER_BONDED_DEVICES) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();

    mojo::Array<BluetoothAddressPtr> bonded_devices =
        mojo::Array<BluetoothAddressPtr>::New(0);

    for (size_t i = 0; i < devices.size(); i++) {
      if (!devices[i]->IsPaired())
        continue;

      BluetoothAddressPtr addr =
          BluetoothAddress::From(devices[i]->GetAddress());
      bonded_devices.push_back(std::move(addr));
    }

    btp->set_bonded_devices(std::move(bonded_devices));
    properties.push_back(std::move(btp));
  }
  if (type == BluetoothPropertyType::ALL ||
      type == BluetoothPropertyType::ADAPTER_DISCOVERY_TIMEOUT) {
    BluetoothPropertyPtr btp = BluetoothProperty::New();
    btp->set_discovery_timeout(120);
    properties.push_back(std::move(btp));
  }

  return properties;
}

void ArcBluetoothBridge::SendCachedDevicesFound() {
  // Send devices that have already been discovered, but aren't connected.
  if (!HasBluetoothInstance())
    return;

  BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
  for (size_t i = 0; i < devices.size(); i++) {
    if (devices[i]->IsPaired())
      continue;

    mojo::Array<BluetoothPropertyPtr> properties =
        GetDeviceProperties(BluetoothPropertyType::ALL, devices[i]);

    arc_bridge_service()->bluetooth_instance()->OnDeviceFound(
        std::move(properties));
  }
}

bool ArcBluetoothBridge::HasBluetoothInstance() {
  if (!arc_bridge_service()->bluetooth_instance()) {
    LOG(WARNING) << "no Bluetooth instance available";
    return false;
  }

  return true;
}

}  // namespace arc
