// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/usb/device_manager_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "device/core/device_client.h"
#include "device/devices_app/usb/device_impl.h"
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

using DeviceList = DeviceManagerImpl::DeviceList;
using DeviceMap = DeviceManagerImpl::DeviceMap;

void OnGetDevicesOnServiceThread(
    const base::Callback<void(const DeviceList&)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const DeviceList& devices) {
  callback_task_runner->PostTask(FROM_HERE, base::Bind(callback, devices));
}

void GetDevicesOnServiceThread(
    const base::Callback<void(const DeviceList&)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (usb_service) {
    usb_service->GetDevices(base::Bind(&OnGetDevicesOnServiceThread, callback,
                                       callback_task_runner));
  } else {
    callback_task_runner->PostTask(FROM_HERE,
                                   base::Bind(callback, DeviceList()));
  }
}

void GetDeviceOnServiceThread(
    const mojo::String& guid,
    const base::Callback<void(scoped_refptr<UsbDevice>)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  DCHECK(DeviceClient::Get());
  scoped_refptr<UsbDevice> device;
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (usb_service)
    device = usb_service->GetDevice(guid);
  callback_task_runner->PostTask(FROM_HERE, base::Bind(callback, device));
}

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

  callback.Run(allowed_devices.Pass());
}

}  // namespace

class DeviceManagerImpl::ServiceThreadHelper
    : public UsbService::Observer,
      public base::MessageLoop::DestructionObserver {
 public:
  ServiceThreadHelper(base::WeakPtr<DeviceManagerImpl> manager,
                      scoped_refptr<base::TaskRunner> task_runner)
      : observer_(this), manager_(manager), task_runner_(task_runner) {}

  ~ServiceThreadHelper() override {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }

  static void Start(scoped_ptr<ServiceThreadHelper> self) {
    UsbService* usb_service = DeviceClient::Get()->GetUsbService();
    if (usb_service)
      self->observer_.Add(usb_service);

    // |self| now owned by the current message loop.
    base::MessageLoop::current()->AddDestructionObserver(self.release());
  }

 private:
  // UsbService::Observer
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DeviceManagerImpl::OnDeviceAdded, manager_, device));
  }

  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DeviceManagerImpl::OnDeviceRemoved, manager_, device));
  }

  void WillDestroyUsbService() override { observer_.RemoveAll(); }

  // base::MessageLoop::DestructionObserver
  void WillDestroyCurrentMessageLoop() override { delete this; }

  ScopedObserver<UsbService, UsbService::Observer> observer_;
  base::WeakPtr<DeviceManagerImpl> manager_;
  scoped_refptr<base::TaskRunner> task_runner_;
};

DeviceManagerImpl::DeviceManagerImpl(
    mojo::InterfaceRequest<DeviceManager> request,
    PermissionProviderPtr permission_provider,
    scoped_refptr<base::SequencedTaskRunner> service_task_runner)
    : permission_provider_(permission_provider.Pass()),
      service_task_runner_(service_task_runner),
      binding_(this, request.Pass()),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if either the message pipe
  // it is bound to is closed or the PermissionProvider it depends on is
  // unavailable.
  binding_.set_connection_error_handler([this]() { delete this; });
  permission_provider_.set_connection_error_handler([this]() { delete this; });

  scoped_ptr<ServiceThreadHelper> helper(new ServiceThreadHelper(
      weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get()));
  helper_ = helper.get();
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceThreadHelper::Start, base::Passed(&helper)));
}

DeviceManagerImpl::~DeviceManagerImpl() {
  // It is safe to call this if |helper_| was already destroyed when
  // |service_task_runner_| exited as the task will never execute.
  service_task_runner_->DeleteSoon(FROM_HERE, helper_);
  connection_error_handler_.Run();
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

void DeviceManagerImpl::GetDeviceChanges(
    const GetDeviceChangesCallback& callback) {
  device_change_callbacks_.push(callback);
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::GetDevice(
    const mojo::String& guid,
    mojo::InterfaceRequest<Device> device_request) {
  auto get_device_callback =
      base::Bind(&DeviceManagerImpl::OnGetDevice, weak_factory_.GetWeakPtr(),
                 base::Passed(&device_request));
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetDeviceOnServiceThread, guid, get_device_callback,
                 base::ThreadTaskRunnerHandle::Get()));
}

void DeviceManagerImpl::OnGetDevice(
    mojo::InterfaceRequest<Device> device_request,
    scoped_refptr<UsbDevice> device) {
  if (!device)
    return;

  mojo::Array<DeviceInfoPtr> requested_devices(1);
  requested_devices[0] = DeviceInfo::From(*device);
  permission_provider_->HasDevicePermission(
      requested_devices.Pass(),
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
  new DeviceImpl(device, permission_provider.Pass(), device_request.Pass());
}

void DeviceManagerImpl::OnGetDevices(EnumerationOptionsPtr options,
                                     const GetDevicesCallback& callback,
                                     const DeviceList& devices) {
  std::vector<UsbDeviceFilter> filters;
  if (options)
    filters = options->filters.To<std::vector<UsbDeviceFilter>>();

  std::map<std::string, scoped_refptr<UsbDevice>> device_map;
  mojo::Array<DeviceInfoPtr> requested_devices(0);
  for (const auto& device : devices) {
    if (filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) {
      device_map[device->guid()] = device;
      requested_devices.push_back(DeviceInfo::From(*device));
    }
  }

  permission_provider_->HasDevicePermission(
      requested_devices.Pass(),
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
        requested_devices.Pass(),
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
    callback.Run(notification.Pass());
    device_change_callbacks_.pop();
  }

  if (devices_added_.size() > 0 || !devices_removed_.empty())
    MaybeRunDeviceChangesCallback();
}

}  // namespace usb
}  // namespace device
