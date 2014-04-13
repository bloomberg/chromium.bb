// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <set>
#include <vector>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_context.h"
#include "chrome/browser/usb/usb_device.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace {

base::LazyInstance<scoped_ptr<UsbService> >::Leaky g_usb_service_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
UsbService* UsbService::GetInstance() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  UsbService* instance = g_usb_service_instance.Get().get();
  if (!instance) {
    PlatformUsbContext context = NULL;
    if (libusb_init(&context) != LIBUSB_SUCCESS)
      return NULL;
    if (!context)
      return NULL;

    instance = new UsbService(context);
    g_usb_service_instance.Get().reset(instance);
  }
  return instance;
}

scoped_refptr<UsbDevice> UsbService::GetDeviceById(uint32 unique_id) {
  DCHECK(CalledOnValidThread());
  RefreshDevices();
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->second->unique_id() == unique_id)
      return it->second;
  }
  return NULL;
}

void UsbService::GetDevices(std::vector<scoped_refptr<UsbDevice> >* devices) {
  DCHECK(CalledOnValidThread());
  STLClearObject(devices);
  RefreshDevices();

  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    devices->push_back(it->second);
  }
}

void UsbService::WillDestroyCurrentMessageLoop() {
  DCHECK(CalledOnValidThread());
  g_usb_service_instance.Get().reset(NULL);
}

UsbService::UsbService(PlatformUsbContext context)
    : context_(new UsbContext(context)), next_unique_id_(0) {
  base::MessageLoop::current()->AddDestructionObserver(this);
}

UsbService::~UsbService() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    it->second->OnDisconnect();
  }
}

void UsbService::RefreshDevices() {
  DCHECK(CalledOnValidThread());

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(context_->context(), &platform_devices);

  std::set<UsbDevice*> connected_devices;
  std::vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    if (!ContainsKey(devices_, platform_devices[i])) {
      libusb_device_descriptor descriptor;
      // This test is needed. A valid vendor/produce pair is required.
      if (0 != libusb_get_device_descriptor(platform_devices[i], &descriptor))
        continue;
      UsbDevice* new_device = new UsbDevice(context_,
                                            platform_devices[i],
                                            descriptor.idVendor,
                                            descriptor.idProduct,
                                            ++next_unique_id_);
      devices_[platform_devices[i]] = new_device;
      connected_devices.insert(new_device);
    } else {
      connected_devices.insert(devices_[platform_devices[i]].get());
    }
  }

  // Find disconnected devices.
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (!ContainsKey(connected_devices, it->second)) {
      disconnected_devices.push_back(it->first);
    }
  }

  // Remove disconnected devices from devices_.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    // UsbDevice will be destroyed after this. The corresponding
    // PlatformUsbDevice will be unref'ed during this process.
    devices_.erase(disconnected_devices[i]);
  }

  libusb_free_device_list(platform_devices, true);
}
