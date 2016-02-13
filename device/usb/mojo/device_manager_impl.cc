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
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

namespace {

using DeviceList = DeviceManagerImpl::DeviceList;
using DeviceMap = DeviceManagerImpl::DeviceMap;

void FilterAndConvertDevicesAndThen(
    const DeviceMap& devices,
    const DeviceManagerImpl::GetDevicesCallback& callback,
    mojo::Array<mojo::String> allowed_guids) {
  mojo::Array<DeviceInfoPtr> allowed_devices(allowed_guids.size());
  for (size_t i = 0; i < allowed_guids.size(); ++i) {
    const auto it = devices.find(allowed_guids[i]);
    DCHECK(it != devices.end());
    allowed_devices[i] = DeviceInfo::From(*it->second);
  }

  callback.Run(std::move(allowed_devices));
}

}  // namespace

// static
void DeviceManagerImpl::Create(PermissionProviderPtr permission_provider,
                               mojo::InterfaceRequest<DeviceManager> request) {
  // The created object is owned by its binding.
  new DeviceManagerImpl(std::move(permission_provider), std::move(request));
}

DeviceManagerImpl::DeviceManagerImpl(
    PermissionProviderPtr permission_provider,
    mojo::InterfaceRequest<DeviceManager> request)
    : permission_provider_(std::move(permission_provider)),
      observer_(this),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if either the message pipe
  // it is bound to is closed or the PermissionProvider it depends on is
  // unavailable.
  binding_.set_connection_error_handler([this]() { delete this; });
  permission_provider_.set_connection_error_handler([this]() { delete this; });

  DCHECK(DeviceClient::Get());
  usb_service_ = DeviceClient::Get()->GetUsbService();
  if (usb_service_)
    observer_.Add(usb_service_);
}

DeviceManagerImpl::~DeviceManagerImpl() {
  connection_error_handler_.Run();
}

void DeviceManagerImpl::GetDevices(EnumerationOptionsPtr options,
                                   const GetDevicesCallback& callback) {
  if (!usb_service_) {
    mojo::Array<DeviceInfoPtr> no_devices;
    callback.Run(std::move(no_devices));
    return;
  }

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
  if (!usb_service_)
    return;

  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  mojo::Array<DeviceInfoPtr> requested_devices(1);
  requested_devices[0] = DeviceInfo::From(*device);
  permission_provider_->HasDevicePermission(
      std::move(requested_devices),
      base::Bind(&DeviceManagerImpl::OnGetDevicePermissionCheckComplete,
                 base::Unretained(this), device,
                 base::Passed(&device_request)));
}

void DeviceManagerImpl::OnGetDevicePermissionCheckComplete(
    scoped_refptr<UsbDevice> device,
    mojo::InterfaceRequest<Device> device_request,
    mojo::Array<mojo::String> allowed_guids) {
  if (allowed_guids.size() == 0)
    return;

  DCHECK(allowed_guids.size() == 1);
  PermissionProviderPtr permission_provider;
  permission_provider_->Bind(mojo::GetProxy(&permission_provider));
  new DeviceImpl(device, std::move(permission_provider),
                 std::move(device_request));
}

void DeviceManagerImpl::OnGetDevices(EnumerationOptionsPtr options,
                                     const GetDevicesCallback& callback,
                                     const DeviceList& devices) {
  std::vector<UsbDeviceFilter> filters;
  if (options)
    filters = options->filters.To<std::vector<UsbDeviceFilter>>();

  std::map<std::string, scoped_refptr<UsbDevice>> device_map;
  mojo::Array<DeviceInfoPtr> requested_devices;
  for (const auto& device : devices) {
    if (filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) {
      device_map[device->guid()] = device;
      requested_devices.push_back(DeviceInfo::From(*device));
    }
  }

  permission_provider_->HasDevicePermission(
      std::move(requested_devices),
      base::Bind(&FilterAndConvertDevicesAndThen, device_map, callback));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK(!ContainsKey(devices_removed_, device->guid()));
  devices_added_[device->guid()] = device;
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  if (devices_added_.erase(device->guid()) == 0)
    devices_removed_[device->guid()] = device;
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::WillDestroyUsbService() {
  observer_.RemoveAll();
  usb_service_ = nullptr;
}

void DeviceManagerImpl::MaybeRunDeviceChangesCallback() {
  if (!permission_request_pending_ && !device_change_callbacks_.empty()) {
    DeviceMap devices_added;
    devices_added.swap(devices_added_);
    DeviceMap devices_removed;
    devices_removed.swap(devices_removed_);

    mojo::Array<DeviceInfoPtr> requested_devices(devices_added.size() +
                                                 devices_removed.size());
    {
      size_t i = 0;
      for (const auto& map_entry : devices_added)
        requested_devices[i++] = DeviceInfo::From(*map_entry.second);
      for (const auto& map_entry : devices_removed)
        requested_devices[i++] = DeviceInfo::From(*map_entry.second);
    }

    permission_request_pending_ = true;
    permission_provider_->HasDevicePermission(
        std::move(requested_devices),
        base::Bind(&DeviceManagerImpl::OnEnumerationPermissionCheckComplete,
                   base::Unretained(this), devices_added, devices_removed));
  }
}

void DeviceManagerImpl::OnEnumerationPermissionCheckComplete(
    const DeviceMap& devices_added,
    const DeviceMap& devices_removed,
    mojo::Array<mojo::String> allowed_guids) {
  permission_request_pending_ = false;

  if (allowed_guids.size() > 0) {
    DeviceChangeNotificationPtr notification = DeviceChangeNotification::New();
    notification->devices_added.resize(0);
    notification->devices_removed.resize(0);

    for (size_t i = 0; i < allowed_guids.size(); ++i) {
      const mojo::String& guid = allowed_guids[i];
      auto it = devices_added.find(guid);
      if (it != devices_added.end()) {
        DCHECK(!ContainsKey(devices_removed, guid));
        notification->devices_added.push_back(DeviceInfo::From(*it->second));
      } else {
        it = devices_removed.find(guid);
        DCHECK(it != devices_removed.end());
        notification->devices_removed.push_back(DeviceInfo::From(*it->second));
      }
    }

    DCHECK(!device_change_callbacks_.empty());
    const GetDeviceChangesCallback& callback = device_change_callbacks_.front();
    callback.Run(std::move(notification));
    device_change_callbacks_.pop();
  }

  if (devices_added_.size() > 0 || !devices_removed_.empty())
    MaybeRunDeviceChangesCallback();
}

}  // namespace usb
}  // namespace device
