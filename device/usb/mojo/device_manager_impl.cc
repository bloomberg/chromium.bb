// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_manager_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "device/core/device_client.h"
#include "device/usb/mojo/device_impl.h"
#include "device/usb/mojo/permission_provider.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

// static
void DeviceManagerImpl::Create(
    base::WeakPtr<PermissionProvider> permission_provider,
    mojo::InterfaceRequest<DeviceManager> request) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (usb_service) {
    new DeviceManagerImpl(permission_provider, usb_service, std::move(request));
  }
}

DeviceManagerImpl::DeviceManagerImpl(
    base::WeakPtr<PermissionProvider> permission_provider,
    UsbService* usb_service,
    mojo::InterfaceRequest<DeviceManager> request)
    : permission_provider_(permission_provider),
      usb_service_(usb_service),
      observer_(this),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if the message pipe it is
  // bound to is closed or the UsbService is shut down.
  binding_.set_connection_error_handler([this]() { delete this; });
  observer_.Add(usb_service_);
}

DeviceManagerImpl::~DeviceManagerImpl() {
  connection_error_handler_.Run();
}

void DeviceManagerImpl::GetDevices(EnumerationOptionsPtr options,
                                   const GetDevicesCallback& callback) {
  usb_service_->GetDevices(base::Bind(&DeviceManagerImpl::OnGetDevices,
                                      weak_factory_.GetWeakPtr(),
                                      base::Passed(&options), callback));
}

void DeviceManagerImpl::GetDeviceChanges(
    const GetDeviceChangesCallback& callback) {
  device_change_callbacks_.push(callback);
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::GetDevice(
    const mojo::String& guid,
    mojo::InterfaceRequest<Device> device_request) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  DeviceInfoPtr device_info = DeviceInfo::From(*device);
  if (permission_provider_ &&
      permission_provider_->HasDevicePermission(*device_info)) {
    new DeviceImpl(device, std::move(device_info), permission_provider_,
                   std::move(device_request));
  }
}

void DeviceManagerImpl::OnGetDevices(
    EnumerationOptionsPtr options,
    const GetDevicesCallback& callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::vector<UsbDeviceFilter> filters;
  if (options)
    filters = options->filters.To<std::vector<UsbDeviceFilter>>();

  mojo::Array<DeviceInfoPtr> device_infos;
  for (const auto& device : devices) {
    if (filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) {
      DeviceInfoPtr device_info = DeviceInfo::From(*device);
      if (permission_provider_ &&
          permission_provider_->HasDevicePermission(*device_info)) {
        device_infos.push_back(std::move(device_info));
      }
    }
  }

  callback.Run(std::move(device_infos));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  DeviceInfoPtr device_info = DeviceInfo::From(*device);
  if (permission_provider_ &&
      permission_provider_->HasDevicePermission(*device_info)) {
    devices_added_[device->guid()] = std::move(device_info);
    MaybeRunDeviceChangesCallback();
  }
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  if (devices_added_.erase(device->guid()) == 0) {
    DeviceInfoPtr device_info = DeviceInfo::From(*device);
    if (permission_provider_ &&
        permission_provider_->HasDevicePermission(*device_info)) {
      devices_removed_.push_back(std::move(device_info));
      MaybeRunDeviceChangesCallback();
    }
  }
}

void DeviceManagerImpl::WillDestroyUsbService() {
  delete this;
}

void DeviceManagerImpl::MaybeRunDeviceChangesCallback() {
  if (!device_change_callbacks_.empty() &&
      !(devices_added_.empty() && devices_removed_.empty())) {
    DeviceChangeNotificationPtr notification = DeviceChangeNotification::New();
    notification->devices_added.SetToEmpty();
    notification->devices_removed.SetToEmpty();
    for (auto& map_entry : devices_added_)
      notification->devices_added.push_back(std::move(map_entry.second));
    devices_added_.clear();
    notification->devices_removed.Swap(&devices_removed_);

    device_change_callbacks_.front().Run(std::move(notification));
    device_change_callbacks_.pop();
  }
}

}  // namespace usb
}  // namespace device
