// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "device/hid/hid_service_mac.h"
#elif defined(OS_WIN)
#include "device/hid/hid_service_win.h"
#endif

namespace device {

void HidService::Observer::OnDeviceAdded(
    scoped_refptr<HidDeviceInfo> device_info) {
}

void HidService::Observer::OnDeviceRemoved(
    scoped_refptr<HidDeviceInfo> device_info) {
}

void HidService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<HidDeviceInfo> device_info) {
}

// static
constexpr base::TaskTraits HidService::kBlockingTaskTraits;

std::unique_ptr<HidService> HidService::Create() {
#if defined(OS_LINUX) && defined(USE_UDEV)
  return base::WrapUnique(new HidServiceLinux());
#elif defined(OS_MACOSX)
  return base::WrapUnique(new HidServiceMac());
#elif defined(OS_WIN)
  return base::WrapUnique(new HidServiceWin());
#else
  return nullptr;
#endif
}

void HidService::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enumeration_ready_) {
    std::vector<scoped_refptr<HidDeviceInfo>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, devices));
  } else {
    pending_enumerations_.push_back(callback);
  }
}

void HidService::AddObserver(HidService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HidService::RemoveObserver(HidService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

scoped_refptr<HidDeviceInfo> HidService::GetDeviceInfo(
    const std::string& device_guid) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::const_iterator it = devices_.find(device_guid);
  if (it == devices_.end()) {
    return nullptr;
  }
  return it->second;
}

HidService::HidService() = default;

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidService::AddDevice(scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string device_guid =
      FindDeviceIdByPlatformDeviceId(device_info->platform_device_id());
  if (device_guid.empty()) {
    devices_[device_info->device_guid()] = device_info;

    HID_LOG(USER) << "HID device "
                  << (enumeration_ready_ ? "added" : "detected")
                  << ": vendorId=" << device_info->vendor_id()
                  << ", productId=" << device_info->product_id() << ", name='"
                  << device_info->product_name() << "', serial='"
                  << device_info->serial_number() << "', deviceId='"
                  << device_info->platform_device_id() << "'";

    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceAdded(device_info);
    }
  }
}

void HidService::RemoveDevice(const HidPlatformDeviceId& platform_device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string device_guid = FindDeviceIdByPlatformDeviceId(platform_device_id);
  if (!device_guid.empty()) {
    HID_LOG(USER) << "HID device removed: deviceId='" << platform_device_id
                  << "'";
    DCHECK(base::ContainsKey(devices_, device_guid));

    scoped_refptr<HidDeviceInfo> device = devices_[device_guid];
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemoved(device);
    }
    devices_.erase(device_guid);
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemovedCleanup(device);
    }
  }
}

void HidService::FirstEnumerationComplete() {
  enumeration_ready_ = true;

  if (!pending_enumerations_.empty()) {
    std::vector<scoped_refptr<HidDeviceInfo>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }

    for (const GetDevicesCallback& callback : pending_enumerations_) {
      callback.Run(devices);
    }
    pending_enumerations_.clear();
  }
}

std::string HidService::FindDeviceIdByPlatformDeviceId(
    const HidPlatformDeviceId& platform_device_id) {
  for (const auto& map_entry : devices_) {
    if (map_entry.second->platform_device_id() == platform_device_id) {
      return map_entry.first;
    }
  }
  return std::string();
}

}  // namespace device
