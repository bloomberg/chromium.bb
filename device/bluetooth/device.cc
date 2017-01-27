// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/public/interfaces/gatt_result_type_converter.h"
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

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    callback.Run(base::nullopt);
    return;
  }

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

void Device::ReadValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    const ReadValueForCharacteristicCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    callback.Run(mojom::GattResult::SERVICE_NOT_FOUND,
                 base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (characteristic == nullptr) {
    callback.Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND,
                 base::nullopt /* value */);
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&Device::OnReadRemoteCharacteristic,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&Device::OnReadRemoteCharacteristicError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void Device::WriteValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::vector<uint8_t>& value,
    const WriteValueForCharacteristicCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    callback.Run(mojom::GattResult::SERVICE_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (characteristic == nullptr) {
    callback.Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND);
    return;
  }

  characteristic->WriteRemoteCharacteristic(
      value, base::Bind(&Device::OnWriteRemoteCharacteristic,
                        weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&Device::OnWriteRemoteCharacteristicError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void Device::GetDescriptors(const std::string& service_id,
                            const std::string& characteristic_id,
                            const GetDescriptorsCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  if (!device) {
    callback.Run(base::nullopt);
    return;
  }

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    callback.Run(base::nullopt);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    callback.Run(base::nullopt);
    return;
  }

  std::vector<mojom::DescriptorInfoPtr> descriptors;

  for (const auto* descriptor : characteristic->GetDescriptors()) {
    mojom::DescriptorInfoPtr descriptor_info = mojom::DescriptorInfo::New();

    descriptor_info->id = descriptor->GetIdentifier();
    descriptor_info->uuid = descriptor->GetUUID();
    descriptor_info->last_known_value = descriptor->GetValue();

    descriptors.push_back(std::move(descriptor_info));
  }

  callback.Run(std::move(descriptors));
}

void Device::ReadValueForDescriptor(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::string& descriptor_id,
    const ReadValueForDescriptorCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    callback.Run(mojom::GattResult::SERVICE_NOT_FOUND,
                 base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    callback.Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND,
                 base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptor(descriptor_id);
  if (!descriptor) {
    callback.Run(mojom::GattResult::DESCRIPTOR_NOT_FOUND,
                 base::nullopt /* value */);
    return;
  }

  descriptor->ReadRemoteDescriptor(
      base::Bind(&Device::OnReadRemoteDescriptor,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&Device::OnReadRemoteDescriptorError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void Device::WriteValueForDescriptor(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::string& descriptor_id,
    const std::vector<uint8_t>& value,
    const WriteValueForDescriptorCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    callback.Run(mojom::GattResult::SERVICE_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    callback.Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptor(descriptor_id);
  if (!descriptor) {
    callback.Run(mojom::GattResult::DESCRIPTOR_NOT_FOUND);
    return;
  }

  descriptor->WriteRemoteDescriptor(
      value, base::Bind(&Device::OnWriteRemoteDescriptor,
                        weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&Device::OnWriteRemoteDescriptorError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

Device::Device(scoped_refptr<device::BluetoothAdapter> adapter,
               std::unique_ptr<device::BluetoothGattConnection> connection)
    : adapter_(std::move(adapter)),
      connection_(std::move(connection)),
      weak_ptr_factory_(this) {
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

void Device::OnReadRemoteCharacteristic(
    const ReadValueForCharacteristicCallback& callback,
    const std::vector<uint8_t>& value) {
  callback.Run(mojom::GattResult::SUCCESS, std::move(value));
}

void Device::OnReadRemoteCharacteristicError(
    const ReadValueForCharacteristicCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  callback.Run(mojo::ConvertTo<mojom::GattResult>(error_code),
               base::nullopt /* value */);
}

void Device::OnWriteRemoteCharacteristic(
    const WriteValueForCharacteristicCallback& callback) {
  callback.Run(mojom::GattResult::SUCCESS);
}

void Device::OnWriteRemoteCharacteristicError(
    const WriteValueForCharacteristicCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  callback.Run(mojo::ConvertTo<mojom::GattResult>(error_code));
}

void Device::OnReadRemoteDescriptor(
    const ReadValueForDescriptorCallback& callback,
    const std::vector<uint8_t>& value) {
  callback.Run(mojom::GattResult::SUCCESS, std::move(value));
}

void Device::OnReadRemoteDescriptorError(
    const ReadValueForDescriptorCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  callback.Run(mojo::ConvertTo<mojom::GattResult>(error_code),
               base::nullopt /* value */);
}

void Device::OnWriteRemoteDescriptor(
    const WriteValueForDescriptorCallback& callback) {
  callback.Run(mojom::GattResult::SUCCESS);
}

void Device::OnWriteRemoteDescriptorError(
    const WriteValueForDescriptorCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  callback.Run(mojo::ConvertTo<mojom::GattResult>(error_code));
}

const std::string& Device::GetAddress() {
  return connection_->GetDeviceAddress();
}

}  // namespace bluetooth
