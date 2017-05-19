// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/base/features.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"

#if defined(OS_ANDROID)
#include "device/usb/usb_service_android.h"
#elif defined(USE_UDEV)
#include "device/usb/usb_service_linux.h"
#else
#if defined(OS_WIN)
#include "device/usb/usb_service_win.h"
#endif
#include "device/usb/usb_service_impl.h"
#endif

namespace device {

UsbService::Observer::~Observer() {}

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::WillDestroyUsbService() {}

// Declare storage for this constexpr.
constexpr base::TaskTraits UsbService::kBlockingTaskTraits;

// static
std::unique_ptr<UsbService> UsbService::Create() {
#if defined(OS_ANDROID)
  return base::WrapUnique(new UsbServiceAndroid());
#elif defined(USE_UDEV)
  return base::WrapUnique(new UsbServiceLinux());
#elif defined(OS_WIN)
  if (base::FeatureList::IsEnabled(kNewUsbBackend))
    return base::WrapUnique(new UsbServiceWin());
  else
    return base::WrapUnique(new UsbServiceImpl());
#elif defined(OS_MACOSX)
  return base::WrapUnique(new UsbServiceImpl());
#else
  return nullptr;
#endif
}

// static
scoped_refptr<base::SequencedTaskRunner>
UsbService::CreateBlockingTaskRunner() {
  return base::CreateSequencedTaskRunnerWithTraits(kBlockingTaskTraits);
}

UsbService::~UsbService() {
  for (auto& observer : observer_list_)
    observer.WillDestroyUsbService();
}

UsbService::UsbService(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(std::move(blocking_task_runner)) {
  if (base::ThreadTaskRunnerHandle::IsSet())
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<UsbDevice> UsbService::GetDevice(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return nullptr;
  return it->second;
}

void UsbService::GetDevices(const GetDevicesCallback& callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  devices.reserve(devices_.size());
  for (const auto& map_entry : devices_)
    devices.push_back(map_entry.second);
  if (task_runner_)
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, devices));
  else
    callback.Run(devices);
}

void UsbService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void UsbService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void UsbService::AddDeviceForTesting(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(devices_, device->guid()));
  devices_[device->guid()] = device;
  testing_devices_.insert(device->guid());
  NotifyDeviceAdded(device);
}

void UsbService::RemoveDeviceForTesting(const std::string& device_guid) {
  DCHECK(CalledOnValidThread());
  // Allow only devices added with AddDeviceForTesting to be removed with this
  // method.
  auto testing_devices_it = testing_devices_.find(device_guid);
  if (testing_devices_it != testing_devices_.end()) {
    auto devices_it = devices_.find(device_guid);
    DCHECK(devices_it != devices_.end());
    scoped_refptr<UsbDevice> device = devices_it->second;
    devices_.erase(devices_it);
    testing_devices_.erase(testing_devices_it);
    UsbService::NotifyDeviceRemoved(device);
  }
}

void UsbService::GetTestDevices(
    std::vector<scoped_refptr<UsbDevice>>* devices) {
  devices->clear();
  devices->reserve(testing_devices_.size());
  for (const std::string& guid : testing_devices_) {
    auto it = devices_.find(guid);
    DCHECK(it != devices_.end());
    devices->push_back(it->second);
  }
}

void UsbService::NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
  device->NotifyDeviceRemoved();
  for (auto& observer : observer_list_)
    observer.OnDeviceRemovedCleanup(device);
}

}  // namespace device
