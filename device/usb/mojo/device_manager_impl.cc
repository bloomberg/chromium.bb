// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "device/base/device_client.h"
#include "device/usb/mojo/device_impl.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/cpp/filter_utils.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"

namespace device {
namespace usb {

// static
std::unique_ptr<DeviceManagerImpl> DeviceManagerImpl::Create(
    mojom::UsbDeviceManagerRequest request) {
  DCHECK(DeviceClient::Get());
  UsbService* service = DeviceClient::Get()->GetUsbService();
  if (!service)
    return nullptr;

  auto* device_manager = new DeviceManagerImpl(service);
  device_manager->binding_.Bind(std::move(request));
  return base::WrapUnique(device_manager);
}

DeviceManagerImpl::DeviceManagerImpl(UsbService* usb_service)
    : usb_service_(usb_service),
      observer_(this),
      binding_(this),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if the message pipe it is
  // bound to is closed, the message loop is destructed, or the UsbService is
  // shut down.
  observer_.Add(usb_service_);
}

DeviceManagerImpl::~DeviceManagerImpl() = default;

void DeviceManagerImpl::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                   GetDevicesCallback callback) {
  usb_service_->GetDevices(
      base::Bind(&DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
                 base::Passed(&options), base::Passed(&callback)));
}

void DeviceManagerImpl::GetDevice(const std::string& guid,
                                  mojom::UsbDeviceRequest device_request,
                                  mojom::UsbDeviceClientPtr device_client) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  DeviceImpl::Create(std::move(device), std::move(device_request),
                     std::move(device_client));
}

void DeviceManagerImpl::SetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);
  client_.Bind(std::move(client));
}

void DeviceManagerImpl::OnGetDevices(
    mojom::UsbEnumerationOptionsPtr options,
    GetDevicesCallback callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& device : devices) {
    if (UsbDeviceFilterMatchesAny(filters, *device)) {
      device_infos.push_back(mojom::UsbDeviceInfo::From(*device));
    }
  }

  std::move(callback).Run(std::move(device_infos));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  if (client_)
    client_->OnDeviceAdded(mojom::UsbDeviceInfo::From(*device));
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  if (client_)
    client_->OnDeviceRemoved(mojom::UsbDeviceInfo::From(*device));
}

void DeviceManagerImpl::WillDestroyUsbService() {
  observer_.RemoveAll();
  binding_.Close();
  client_.reset();
}

}  // namespace usb
}  // namespace device
