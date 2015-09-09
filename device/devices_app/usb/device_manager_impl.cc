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

void OnGetDevicesOnServiceThread(
    const std::vector<UsbDeviceFilter>& filters,
    const base::Callback<void(mojo::Array<DeviceInfoPtr>)>& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  mojo::Array<DeviceInfoPtr> mojo_devices(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (UsbDeviceFilter::MatchesAny(devices[i], filters) || filters.empty())
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

void FilterDeviceListAndThen(
    const DeviceManagerImpl::GetDevicesCallback& callback,
    mojo::Array<DeviceInfoPtr> devices,
    mojo::Array<mojo::String> allowed_guids) {
  std::set<std::string> allowed_guid_set;
  for (size_t i = 0; i < allowed_guids.size(); ++i)
    allowed_guid_set.insert(allowed_guids[i]);

  mojo::Array<DeviceInfoPtr> allowed_devices(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (ContainsKey(allowed_guid_set, devices[i]->guid))
      allowed_devices.push_back(devices[i].Pass());
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
    DeviceInfoPtr mojo_device(DeviceInfo::From(*device));
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeviceManagerImpl::OnDeviceAdded, manager_,
                              base::Passed(&mojo_device)));
  }

  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeviceManagerImpl::OnDeviceRemoved, manager_,
                              device->guid()));
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
  std::vector<UsbDeviceFilter> filters;
  if (options)
    filters = options->filters.To<std::vector<UsbDeviceFilter>>();
  auto get_devices_callback = base::Bind(&DeviceManagerImpl::OnGetDevices,
                                         weak_factory_.GetWeakPtr(), callback);
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetDevicesOnServiceThread, filters, get_devices_callback,
                 base::ThreadTaskRunnerHandle::Get()));
}

void DeviceManagerImpl::GetDeviceChanges(
    const GetDeviceChangesCallback& callback) {
  device_change_callbacks_.push(callback);
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::OpenDevice(
    const mojo::String& guid,
    mojo::InterfaceRequest<Device> device_request,
    const OpenDeviceCallback& callback) {
  mojo::Array<mojo::String> requested_guids(1);
  requested_guids[0] = guid;
  permission_provider_->HasDevicePermission(
      requested_guids.Pass(),
      base::Bind(&DeviceManagerImpl::OnOpenDevicePermissionCheckComplete,
                 base::Unretained(this), base::Passed(&device_request),
                 callback));
}

void DeviceManagerImpl::OnOpenDevicePermissionCheckComplete(
    mojo::InterfaceRequest<Device> device_request,
    const OpenDeviceCallback& callback,
    mojo::Array<mojo::String> allowed_guids) {
  if (allowed_guids.size() == 0) {
    callback.Run(OPEN_DEVICE_ERROR_ACCESS_DENIED);
    return;
  }

  DCHECK(allowed_guids.size() == 1);
  service_task_runner_->PostTask(
      FROM_HERE, base::Bind(&OpenDeviceOnServiceThread, allowed_guids[0],
                            base::Passed(&device_request), callback,
                            base::ThreadTaskRunnerHandle::Get()));
}

void DeviceManagerImpl::OnGetDevices(const GetDevicesCallback& callback,
                                     mojo::Array<DeviceInfoPtr> devices) {
  mojo::Array<mojo::String> requested_guids(devices.size());
  for (size_t i = 0; i < devices.size(); ++i)
    requested_guids[i] = devices[i]->guid;

  permission_provider_->HasDevicePermission(
      requested_guids.Pass(),
      base::Bind(&FilterDeviceListAndThen, callback, base::Passed(&devices)));
}

void DeviceManagerImpl::OnDeviceAdded(DeviceInfoPtr device) {
  DCHECK(!ContainsKey(devices_removed_, device->guid));
  devices_added_.push_back(device.Pass());
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::OnDeviceRemoved(std::string device_guid) {
  bool found = false;
  mojo::Array<DeviceInfoPtr> devices_added;
  for (size_t i = 0; i < devices_added_.size(); ++i) {
    if (devices_added_[i]->guid == device_guid)
      found = true;
    else
      devices_added.push_back(devices_added_[i].Pass());
  }
  devices_added.Swap(&devices_added_);
  if (!found)
    devices_removed_.insert(device_guid);
  MaybeRunDeviceChangesCallback();
}

void DeviceManagerImpl::MaybeRunDeviceChangesCallback() {
  if (!permission_request_pending_ && !device_change_callbacks_.empty()) {
    mojo::Array<DeviceInfoPtr> devices_added;
    devices_added.Swap(&devices_added_);
    std::set<std::string> devices_removed;
    devices_removed.swap(devices_removed_);

    mojo::Array<mojo::String> requested_guids(devices_added.size() +
                                              devices_removed.size());
    {
      size_t i;
      for (i = 0; i < devices_added.size(); ++i)
        requested_guids[i] = devices_added[i]->guid;
      for (const std::string& guid : devices_removed)
        requested_guids[i++] = guid;
    }

    permission_request_pending_ = true;
    permission_provider_->HasDevicePermission(
        requested_guids.Pass(),
        base::Bind(&DeviceManagerImpl::OnEnumerationPermissionCheckComplete,
                   base::Unretained(this), base::Passed(&devices_added),
                   devices_removed));
  }
}

void DeviceManagerImpl::OnEnumerationPermissionCheckComplete(
    mojo::Array<DeviceInfoPtr> devices_added,
    const std::set<std::string>& devices_removed,
    mojo::Array<mojo::String> allowed_guids) {
  permission_request_pending_ = false;

  if (allowed_guids.size() > 0) {
    std::set<std::string> allowed_guid_set;
    for (size_t i = 0; i < allowed_guids.size(); ++i)
      allowed_guid_set.insert(allowed_guids[i]);

    DeviceChangeNotificationPtr notification = DeviceChangeNotification::New();
    notification->devices_added.resize(0);
    for (size_t i = 0; i < devices_added.size(); ++i) {
      if (ContainsKey(allowed_guid_set, devices_added[i]->guid))
        notification->devices_added.push_back(devices_added[i].Pass());
    }

    notification->devices_removed.resize(0);
    for (const std::string& guid : devices_removed) {
      if (ContainsKey(allowed_guid_set, guid))
        notification->devices_removed.push_back(guid);
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
