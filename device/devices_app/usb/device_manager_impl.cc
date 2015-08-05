// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/usb/device_manager_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "device/core/device_client.h"
#include "device/devices_app/usb/device_impl.h"
#include "device/devices_app/usb/public/cpp/device_manager_delegate.h"
#include "device/devices_app/usb/public/interfaces/device.mojom.h"
#include "device/devices_app/usb/type_converters.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

namespace {

void OnGetDevicesOnServiceThread(
    const UsbService::GetDevicesCallback& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  callback_task_runner->PostTask(FROM_HERE, base::Bind(callback, devices));
}

void GetDevicesOnServiceThread(
    const UsbService::GetDevicesCallback& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (!usb_service) {
    std::vector<scoped_refptr<UsbDevice>> no_devices;
    callback_task_runner->PostTask(FROM_HERE, base::Bind(callback, no_devices));
    return;
  }
  usb_service->GetDevices(
      base::Bind(&OnGetDevicesOnServiceThread, callback, callback_task_runner));
}

void RunOpenDeviceCallback(const DeviceManager::OpenDeviceCallback& callback,
                           OpenDeviceError error) {
  callback.Run(error);
}

void OnOpenDeviceOnServiceThread(
    mojo::InterfaceRequest<Device> device_request,
    const DeviceManager::OpenDeviceCallback& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (!device_handle) {
    callback_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&RunOpenDeviceCallback, callback,
                                              OPEN_DEVICE_ERROR_ACCESS_DENIED));
    return;
  }

  // Owned by its MessagePipe.
  new DeviceImpl(device_handle, device_request.Pass());

  callback_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RunOpenDeviceCallback, callback, OPEN_DEVICE_ERROR_OK));
}

void OpenDeviceOnServiceThread(
    const std::string& guid,
    mojo::InterfaceRequest<Device> device_request,
    const DeviceManager::OpenDeviceCallback& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (!usb_service) {
    callback_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&RunOpenDeviceCallback, callback,
                                              OPEN_DEVICE_ERROR_NOT_FOUND));
    return;
  }
  scoped_refptr<UsbDevice> device = usb_service->GetDevice(guid);
  if (!device) {
    callback_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&RunOpenDeviceCallback, callback,
                                              OPEN_DEVICE_ERROR_NOT_FOUND));
    return;
  }
  device->Open(base::Bind(&OnOpenDeviceOnServiceThread,
                          base::Passed(&device_request), callback,
                          callback_task_runner));
}

}  // namespace

DeviceManagerImpl::DeviceManagerImpl(
    mojo::InterfaceRequest<DeviceManager> request,
    scoped_ptr<DeviceManagerDelegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> service_task_runner)
    : binding_(this, request.Pass()),
      delegate_(delegate.Pass()),
      service_task_runner_(service_task_runner),
      weak_factory_(this) {
}

DeviceManagerImpl::~DeviceManagerImpl() {
}

void DeviceManagerImpl::set_connection_error_handler(
    const mojo::Closure& error_handler) {
  binding_.set_connection_error_handler(error_handler);
}

void DeviceManagerImpl::GetDevices(EnumerationOptionsPtr options,
                                   const GetDevicesCallback& callback) {
  auto get_devices_callback =
      base::Bind(&DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
                 base::Passed(&options), callback);
  service_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GetDevicesOnServiceThread, get_devices_callback,
                            base::ThreadTaskRunnerHandle::Get()));
}

void DeviceManagerImpl::OpenDevice(
    const mojo::String& guid,
    mojo::InterfaceRequest<Device> device_request,
    const OpenDeviceCallback& callback) {
  service_task_runner_->PostTask(
      FROM_HERE, base::Bind(&OpenDeviceOnServiceThread, guid,
                            base::Passed(&device_request), callback,
                            base::ThreadTaskRunnerHandle::Get()));
}

void DeviceManagerImpl::OnGetDevices(
    EnumerationOptionsPtr options,
    const GetDevicesCallback& callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  auto filters = options->filters.To<std::vector<UsbDeviceFilter>>();
  mojo::Array<DeviceInfoPtr> device_infos(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    DeviceInfoPtr device_info = DeviceInfo::From(*devices[i]);
    if (UsbDeviceFilter::MatchesAny(devices[i], filters) &&
        delegate_->IsDeviceAllowed(*device_info)) {
      const UsbConfigDescriptor* config = devices[i]->GetActiveConfiguration();
      device_info->configurations = mojo::Array<ConfigurationInfoPtr>::New(0);
      if (config)
        device_info->configurations.push_back(ConfigurationInfo::From(*config));
      device_infos.push_back(device_info.Pass());
    }
  }
  callback.Run(device_infos.Pass());
}

}  // namespace usb
}  // namespace device
