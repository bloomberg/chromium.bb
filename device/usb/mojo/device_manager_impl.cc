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
#include "device/usb/public/cpp/usb_utils.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "device/usb/usb_device_linux.h"
#endif  // defined(OS_CHROMEOS)

namespace device {
namespace usb {

DeviceManagerImpl::DeviceManagerImpl() : observer_(this), weak_factory_(this) {
  usb_service_ = DeviceClient::Get()->GetUsbService();
  if (usb_service_)
    observer_.Add(usb_service_);
}

DeviceManagerImpl::~DeviceManagerImpl() = default;

void DeviceManagerImpl::AddBinding(mojom::UsbDeviceManagerRequest request) {
  if (usb_service_)
    bindings_.AddBinding(this, std::move(request));
}

void DeviceManagerImpl::EnumerateDevicesAndSetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
    EnumerateDevicesAndSetClientCallback callback) {
  usb_service_->GetDevices(base::Bind(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      /*options=*/nullptr, base::Passed(&client), base::Passed(&callback)));
}

void DeviceManagerImpl::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                   GetDevicesCallback callback) {
  usb_service_->GetDevices(base::Bind(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      base::Passed(&options), /*client=*/nullptr, base::Passed(&callback)));
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

#if defined(OS_CHROMEOS)
void DeviceManagerImpl::CheckAccess(const std::string& guid,
                                    CheckAccessCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (device) {
    device->CheckUsbAccess(std::move(callback));
  } else {
    LOG(ERROR) << "Was asked to check access to non-existent USB device: "
               << guid;
    std::move(callback).Run(false);
  }
}

void DeviceManagerImpl::OpenFileDescriptor(
    const std::string& guid,
    OpenFileDescriptorCallback callback) {
  auto* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device) {
    LOG(ERROR) << "Was asked to open non-existent USB device: " << guid;
    std::move(callback).Run(base::File());
  } else {
    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    auto devpath =
        static_cast<device::UsbDeviceLinux*>(device.get())->device_path();
    client->OpenPath(
        devpath,
        base::BindRepeating(&DeviceManagerImpl::OnOpenFileDescriptor,
                            weak_factory_.GetWeakPtr(), copyable_callback),
        base::BindRepeating(&DeviceManagerImpl::OnOpenFileDescriptorError,
                            weak_factory_.GetWeakPtr(), copyable_callback));
  }
}

void DeviceManagerImpl::OnOpenFileDescriptor(
    OpenFileDescriptorCallback callback,
    base::ScopedFD fd) {
  std::move(callback).Run(base::File(fd.release()));
}

void DeviceManagerImpl::OnOpenFileDescriptorError(
    OpenFileDescriptorCallback callback,
    const std::string& error_name,
    const std::string& message) {
  LOG(ERROR) << "Failed to open USB device file: " << error_name << " "
             << message;
  std::move(callback).Run(base::File());
}
#endif  // defined(OS_CHROMEOS)

void DeviceManagerImpl::SetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);
  mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void DeviceManagerImpl::OnGetDevices(
    mojom::UsbEnumerationOptionsPtr options,
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
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

  if (client)
    SetClient(std::move(client));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  clients_.ForAllPtrs([&device_info](mojom::UsbDeviceManagerClient* client) {
    client->OnDeviceAdded(device_info->Clone());
  });
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  clients_.ForAllPtrs([&device_info](mojom::UsbDeviceManagerClient* client) {
    client->OnDeviceRemoved(device_info->Clone());
  });
}

void DeviceManagerImpl::WillDestroyUsbService() {
  observer_.RemoveAll();
  usb_service_ = nullptr;

  // Close all the connections.
  bindings_.CloseAllBindings();
  clients_.CloseAll();
}

}  // namespace usb
}  // namespace device
