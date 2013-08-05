// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/usb/usb_context.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

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
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_APP_TERMINATING) {
      registrar_.RemoveAll();
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, service_);
    }
  }
  UsbService* service_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

UsbService::UsbService() : context_(new UsbContext()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

UsbService::~UsbService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // UsbDeviceHandle::Close removes itself from devices_.
  while (devices_.size())
    devices_.begin()->second->Close();
}

UsbService* UsbService::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // UsbService deletes itself upon APP_TERMINATING.
  return Singleton<UsbService, LeakySingletonTraits<UsbService> >::get();
}

void UsbService::FindDevices(
    const uint16 vendor_id,
    const uint16 product_id,
    int interface_id,
    const base::Callback<void(ScopedDeviceVector vector)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
#if defined(OS_CHROMEOS)
  // ChromeOS builds on non-ChromeOS machines (dev) should not attempt to
  // use permission broker.
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::PermissionBrokerClient* client =
        chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
    DCHECK(client) << "Could not get permission broker client.";
    if (!client) {
      callback.Run(ScopedDeviceVector());
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&chromeos::PermissionBrokerClient::RequestUsbAccess,
                   base::Unretained(client),
                   vendor_id,
                   product_id,
                   interface_id,
                   base::Bind(&UsbService::OnRequestUsbAccessReplied,
                              base::Unretained(this),
                              vendor_id,
                              product_id,
                              callback)));
  } else {
    FindDevicesImpl(vendor_id, product_id, callback, true);
  }
#else
  FindDevicesImpl(vendor_id, product_id, callback, true);
#endif  // defined(OS_CHROMEOS)
}

void UsbService::EnumerateDevices(
    vector<scoped_refptr<UsbDeviceHandle> >* devices) {
  devices->clear();

  DeviceVector enumerated_devices;
  EnumerateDevicesImpl(&enumerated_devices);

  for (DeviceVector::iterator it = enumerated_devices.begin();
       it != enumerated_devices.end(); ++it) {
    PlatformUsbDevice device = it->device();
    UsbDeviceHandle* const wrapper = LookupOrCreateDevice(device);
    if (wrapper)
      devices->push_back(wrapper);
  }
}

void UsbService::OnRequestUsbAccessReplied(
    const uint16 vendor_id,
    const uint16 product_id,
    const base::Callback<void(ScopedDeviceVector vectors)>& callback,
    bool success) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbService::FindDevicesImpl,
                 base::Unretained(this),
                 vendor_id,
                 product_id,
                 callback,
                 success));
}

void UsbService::FindDevicesImpl(
    const uint16 vendor_id,
    const uint16 product_id,
    const base::Callback<void(ScopedDeviceVector vectors)>& callback,
    bool success) {
  ScopedDeviceVector devices(new vector<scoped_refptr<UsbDeviceHandle> >());

  // If the permission broker was unable to obtain permission for the specified
  // devices then there is no point in attempting to enumerate the devices. On
  // platforms without a permission broker, we assume permission is granted.
  if (!success) {
    callback.Run(devices.Pass());
    return;
  }

  DeviceVector enumerated_devices;
  EnumerateDevicesImpl(&enumerated_devices);

  for (DeviceVector::iterator it = enumerated_devices.begin();
       it != enumerated_devices.end(); ++it) {
    PlatformUsbDevice device = it->device();
    if (DeviceMatches(device, vendor_id, product_id)) {
      UsbDeviceHandle* const wrapper = LookupOrCreateDevice(device);
      if (wrapper)
        devices->push_back(make_scoped_refptr(wrapper));
    }
  }
  callback.Run(devices.Pass());
}

void UsbService::CloseDevice(scoped_refptr<UsbDeviceHandle> device) {
  PlatformUsbDevice platform_device = libusb_get_device(device->handle());
  if (!ContainsKey(devices_, platform_device)) {
    LOG(WARNING) << "CloseDevice called for device we're not tracking!";
    return;
  }

  devices_.erase(platform_device);
  libusb_close(device->handle());
}

UsbService::RefCountedPlatformUsbDevice::RefCountedPlatformUsbDevice(
    PlatformUsbDevice device) : device_(device) {
  libusb_ref_device(device_);
}

UsbService::RefCountedPlatformUsbDevice::RefCountedPlatformUsbDevice(
    const RefCountedPlatformUsbDevice& other) : device_(other.device_) {
  libusb_ref_device(device_);
}

UsbService::RefCountedPlatformUsbDevice::~RefCountedPlatformUsbDevice() {
  libusb_unref_device(device_);
}

PlatformUsbDevice UsbService::RefCountedPlatformUsbDevice::device() {
  return device_;
}

void UsbService::EnumerateDevicesImpl(DeviceVector* output) {
  STLClearObject(output);

  libusb_device** devices = NULL;
  const ssize_t device_count = libusb_get_device_list(
      context_->context(),
      &devices);
  if (device_count < 0)
    return;

  for (int i = 0; i < device_count; ++i) {
    libusb_device* device = devices[i];
    libusb_ref_device(device);
    output->push_back(RefCountedPlatformUsbDevice(device));
  }

  libusb_free_device_list(devices, true);
}

bool UsbService::DeviceMatches(PlatformUsbDevice device,
                               const uint16 vendor_id,
                               const uint16 product_id) {
  libusb_device_descriptor descriptor;
  if (libusb_get_device_descriptor(device, &descriptor))
    return false;
  return descriptor.idVendor == vendor_id && descriptor.idProduct == product_id;
}

UsbDeviceHandle* UsbService::LookupOrCreateDevice(PlatformUsbDevice device) {
  if (!ContainsKey(devices_, device)) {
    libusb_device_handle* handle = NULL;
    if (libusb_open(device, &handle)) {
      LOG(WARNING) << "Could not open device.";
      return NULL;
    }

    UsbDeviceHandle* wrapper = new UsbDeviceHandle(this, handle);
    devices_[device] = wrapper;
  }
  return devices_[device].get();
}
