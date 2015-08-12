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
    const std::vector<UsbDeviceFilter>& filters,
    const base::Callback<void(mojo::Array<DeviceInfoPtr>)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  mojo::Array<DeviceInfoPtr> mojo_devices(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (UsbDeviceFilter::MatchesAny(devices[i], filters))
      mojo_devices.push_back(DeviceInfo::From(*devices[i]));
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&mojo_devices)));
}

void GetDevicesOnServiceThread(
    const std::vector<UsbDeviceFilter>& filters,
    const base::Callback<void(mojo::Array<DeviceInfoPtr>)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (!usb_service) {
    mojo::Array<DeviceInfoPtr> no_devices(0);
    callback_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&no_devices)));
    return;
  }
  usb_service->GetDevices(base::Bind(&OnGetDevicesOnServiceThread, filters,
                                     callback, callback_task_runner));
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
  auto filters = options->filters.To<std::vector<UsbDeviceFilter>>();
  auto get_devices_callback = base::Bind(&DeviceManagerImpl::OnGetDevices,
                                         weak_factory_.GetWeakPtr(), callback);
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetDevicesOnServiceThread, filters, get_devices_callback,
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

void DeviceManagerImpl::OnGetDevices(const GetDevicesCallback& callback,
                                     mojo::Array<DeviceInfoPtr> devices) {
  mojo::Array<DeviceInfoPtr> allowed_devices(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (delegate_->IsDeviceAllowed(*devices[i]))
      allowed_devices.push_back(devices[i].Pass());
  }
  callback.Run(allowed_devices.Pass());
}

}  // namespace usb
}  // namespace device
