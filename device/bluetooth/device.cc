// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {
Device::~Device() {
  adapter_->RemoveObserver(this);
}

// static
void Device::Create(scoped_refptr<device::BluetoothAdapter> adapter,
                    std::unique_ptr<device::BluetoothGattConnection> connection,
                    mojom::DeviceRequest request) {
  auto device_impl =
      base::WrapUnique(new Device(adapter, std::move(connection)));
  auto* device_ptr = device_impl.get();
  device_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(device_impl), std::move(request));
}

// static
mojom::DeviceInfoPtr Device::ConstructDeviceInfoStruct(
    const device::BluetoothDevice* device) {
  mojom::DeviceInfoPtr device_info = mojom::DeviceInfo::New();

  device_info->name = device->GetName();
  device_info->name_for_display =
      base::UTF16ToUTF8(device->GetNameForDisplay());
  device_info->address = device->GetAddress();
  device_info->is_gatt_connected = device->IsGattConnected();

  if (device->GetInquiryRSSI()) {
    device_info->rssi = mojom::RSSIWrapper::New();
    device_info->rssi->value = device->GetInquiryRSSI().value();
  }

  return device_info;
}

void Device::DeviceChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) {
  if (device->GetAddress() != GetAddress()) {
    return;
  }

  if (!device->IsGattConnected()) {
    binding_->Close();
  }
}

void Device::GattServicesDiscovered(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  if (device->GetAddress() != GetAddress()) {
    return;
  }

  std::vector<base::Closure> requests;
  requests.swap(pending_services_requests_);
  for (const base::Closure& request : requests) {
    request.Run();
  }
}

void Device::Disconnect() {
  binding_->Close();
}

void Device::GetInfo(const GetInfoCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  callback.Run(ConstructDeviceInfoStruct(device));
}

void Device::GetServices(const GetServicesCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  if (device->IsGattServicesDiscoveryComplete()) {
    GetServicesImpl(callback);
    return;
  }

  // pending_services_requests_ is owned by Device, so base::Unretained is
  // safe.
  pending_services_requests_.push_back(
      base::Bind(&Device::GetServicesImpl, base::Unretained(this), callback));
}

void Device::GetCharacteristics(const std::string& service_id,
                                const GetCharacteristicsCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  if (device->IsGattServicesDiscoveryComplete()) {
    GetCharacteristicsImpl(service_id, callback);
    return;
  }

  // pending_services_requests_ is owned by Device, so base::Unretained is
  // safe.
  pending_services_requests_.push_back(
      base::Bind(&Device::GetCharacteristicsImpl, base::Unretained(this),
                 service_id, callback));
}

Device::Device(scoped_refptr<device::BluetoothAdapter> adapter,
               std::unique_ptr<device::BluetoothGattConnection> connection)
    : adapter_(std::move(adapter)), connection_(std::move(connection)) {
  adapter_->AddObserver(this);
}

void Device::GetServicesImpl(const GetServicesCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  std::vector<mojom::ServiceInfoPtr> services;

  for (const device::BluetoothRemoteGattService* service :
       device->GetGattServices()) {
    services.push_back(ConstructServiceInfoStruct(*service));
  }

  callback.Run(std::move(services));
}

mojom::ServiceInfoPtr Device::ConstructServiceInfoStruct(
    const device::BluetoothRemoteGattService& service) {
  mojom::ServiceInfoPtr service_info = mojom::ServiceInfo::New();

  service_info->id = service.GetIdentifier();
  service_info->uuid = service.GetUUID();
  service_info->is_primary = service.IsPrimary();

  return service_info;
}

void Device::GetCharacteristicsImpl(
    const std::string& service_id,
    const GetCharacteristicsCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);
  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  DCHECK(service);

  std::vector<mojom::CharacteristicInfoPtr> characteristics;

  for (const auto* characteristic : service->GetCharacteristics()) {
    mojom::CharacteristicInfoPtr characteristic_info =
        mojom::CharacteristicInfo::New();

    characteristic_info->id = characteristic->GetIdentifier();
    characteristic_info->uuid = characteristic->GetUUID();
    characteristic_info->properties = characteristic->GetProperties();

    characteristics.push_back(std::move(characteristic_info));
  }

  callback.Run(std::move(characteristics));
}

const std::string& Device::GetAddress() {
  return connection_->GetDeviceAddress();
}

}  // namespace bluetooth
