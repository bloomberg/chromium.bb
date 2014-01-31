// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "device/hid/hid_device_info.h"

#if defined(OS_LINUX)
#include "device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "device/hid/hid_service_mac.h"
#else
#include "device/hid/hid_service_win.h"
#endif

namespace device {

namespace {

// The instance will be reset when message loop destroys.
base::LazyInstance<scoped_ptr<HidService> >::Leaky g_hid_service_ptr =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

HidService::HidService() : initialized_(false) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->AddDestructionObserver(this);
}

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void HidService::WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_hid_service_ptr.Get().reset(NULL);
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
bool HidService::GetInfo(std::string device_id, HidDeviceInfo* info) const {
  DeviceMap::const_iterator it = devices_.find(device_id);
  if (it == devices_.end())
    return false;
  *info = it->second;
  return true;
}

void HidService::AddDevice(HidDeviceInfo info) {
  if (!ContainsKey(devices_, info.device_id)) {
    DCHECK(thread_checker_.CalledOnValidThread());
    devices_[info.device_id] = info;
  }
}

void HidService::RemoveDevice(std::string device_id) {
  if (ContainsKey(devices_, device_id)) {
    DCHECK(thread_checker_.CalledOnValidThread());
    devices_.erase(device_id);
  }
}

HidService* HidService::CreateInstance() {
#if defined(OS_LINUX)
    return new HidServiceLinux();
#elif defined(OS_MACOSX)
    return new HidServiceMac();
#elif defined(OS_WIN)
    return new HidServiceWin();
#else
    return NULL;
#endif
}

HidService* HidService::GetInstance() {
  if (!g_hid_service_ptr.Get().get()){
    scoped_ptr<HidService> service(CreateInstance());

    if (service && service->initialized())
      g_hid_service_ptr.Get().reset(service.release());
  }
  return g_hid_service_ptr.Get().get();
}

}  // namespace device
