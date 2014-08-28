// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "device/hid/hid_service_mac.h"
#else
#include "device/hid/hid_service_win.h"
#endif

namespace device {

HidService* HidService::Create(
    scoped_refptr<base::MessageLoopProxy> ui_message_loop) {
#if defined(OS_LINUX) && defined(USE_UDEV)
  return new HidServiceLinux(ui_message_loop);
#elif defined(OS_MACOSX)
  return new HidServiceMac();
#elif defined(OS_WIN)
  return new HidServiceWin();
#else
  return NULL;
#endif
}

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidService::GetDevices(std::vector<HidDeviceInfo>* devices) {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLClearObject(devices);
  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end();
      ++it) {
    devices->push_back(it->second);
  }
}

// Fills in the device info struct of the given device_id.
bool HidService::GetDeviceInfo(const HidDeviceId& device_id,
                               HidDeviceInfo* info) const {
  DeviceMap::const_iterator it = devices_.find(device_id);
  if (it == devices_.end())
    return false;
  *info = it->second;
  return true;
}

HidService::HidService() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidService::AddDevice(const HidDeviceInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ContainsKey(devices_, info.device_id)) {
    devices_[info.device_id] = info;
  }
}

void HidService::RemoveDevice(const HidDeviceId& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::iterator it = devices_.find(device_id);
  if (it != devices_.end())
    devices_.erase(it);
}

const HidService::DeviceMap& HidService::GetDevicesNoEnumerate() const {
  return devices_;
}

}  // namespace device
