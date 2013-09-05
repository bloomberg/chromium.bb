// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/usb/usb_context.h"
#include "chrome/browser/usb/usb_device.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace content {

class NotificationDetails;
class NotificationSource;

}  // namespace content

using content::BrowserThread;
using std::vector;

namespace {

class ExitObserver : public content::NotificationObserver {
 public:
  explicit ExitObserver(UsbService* service) : service_(service) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&content::NotificationRegistrar::Add,
                   base::Unretained(&registrar_), this,
                   chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources()));
  }

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_APP_TERMINATING) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, service_);
      delete this;
    }
  }
  UsbService* service_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

using content::BrowserThread;

UsbService::UsbService()
    : context_(new UsbContext()),
      next_unique_id_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Will be deleted upon NOTIFICATION_APP_TERMINATING.
  new ExitObserver(this);
}

UsbService::~UsbService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    it->second->OnDisconnect();
  }
}

UsbService* UsbService::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // UsbService deletes itself upon APP_TERMINATING.
  return Singleton<UsbService, LeakySingletonTraits<UsbService> >::get();
}

void UsbService::GetDevices(std::vector<scoped_refptr<UsbDevice> >* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  STLClearObject(devices);
  RefreshDevices();

  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    devices->push_back(it->second);
  }
}

scoped_refptr<UsbDevice> UsbService::GetDeviceById(uint32 unique_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  RefreshDevices();

  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->second->unique_id() == unique_id) return it->second;
  }
  return NULL;
}

void UsbService::RefreshDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(context_->context(), &platform_devices);

  std::set<UsbDevice*> connected_devices;
  vector<PlatformUsbDevice> disconnected_devices;

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
