// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "device/hid/hid_service_mac.h"
#else
#include "device/hid/hid_service_win.h"
#endif

namespace device {

namespace {

HidService* g_service = NULL;

}  // namespace

class HidService::Destroyer : public base::MessageLoop::DestructionObserver {
 public:
  explicit Destroyer(HidService* hid_service)
      : hid_service_(hid_service) {}
  ~Destroyer() override {}

 private:
  // base::MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
    delete hid_service_;
    delete this;
    g_service = NULL;
  }

  HidService* hid_service_;
};

HidService* HidService::GetInstance(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  if (g_service == NULL) {
#if defined(OS_LINUX) && defined(USE_UDEV)
    g_service = new HidServiceLinux(file_task_runner);
#elif defined(OS_MACOSX)
    g_service = new HidServiceMac(file_task_runner);
#elif defined(OS_WIN)
    g_service = new HidServiceWin();
#endif
    if (g_service != nullptr) {
      Destroyer* destroyer = new Destroyer(g_service);
      base::MessageLoop::current()->AddDestructionObserver(destroyer);
    }
  }
  return g_service;
}

void HidService::SetInstanceForTest(HidService* instance) {
  DCHECK(!g_service);
  g_service = instance;
  Destroyer* destroyer = new Destroyer(g_service);
  base::MessageLoop::current()->AddDestructionObserver(destroyer);
}

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidService::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enumeration_ready_) {
    std::vector<HidDeviceInfo> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, devices));
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

// Fills in the device info struct of the given device_id.
bool HidService::GetDeviceInfo(const HidDeviceId& device_id,
                               HidDeviceInfo* info) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::const_iterator it = devices_.find(device_id);
  if (it == devices_.end())
    return false;
  *info = it->second;
  return true;
}

HidService::HidService() : enumeration_ready_(false) {
}

void HidService::AddDevice(const HidDeviceInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ContainsKey(devices_, info.device_id)) {
    devices_[info.device_id] = info;

    if (enumeration_ready_) {
      FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(info));
    }
  }
}

void HidService::RemoveDevice(const HidDeviceId& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::iterator it = devices_.find(device_id);
  if (it != devices_.end()) {
    if (enumeration_ready_) {
      FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(it->second));
    }
    devices_.erase(it);
  }
}

void HidService::FirstEnumerationComplete() {
  enumeration_ready_ = true;

  if (!pending_enumerations_.empty()) {
    std::vector<HidDeviceInfo> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }

    for (const GetDevicesCallback& callback : pending_enumerations_) {
      callback.Run(devices);
    }
    pending_enumerations_.clear();
  }
}

}  // namespace device
