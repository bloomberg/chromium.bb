// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_device.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using std::vector;

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(PlatformUsbContext context)
      : running_(true), context_(context) {
    base::PlatformThread::CreateNonJoinable(0, this);
  }

  virtual ~UsbEventHandler() {}

  virtual void ThreadMain() {
    base::PlatformThread::SetName("UsbEventHandler");

    DLOG(INFO) << "UsbEventHandler started.";
    while (running_) {
      libusb_handle_events(context_);
    }
    DLOG(INFO) << "UsbEventHandler shutting down.";
    libusb_exit(context_);

    delete this;
  }

  void Stop() {
    running_ = false;
  }

 private:
  bool running_;
  PlatformUsbContext context_;

  DISALLOW_EVIL_CONSTRUCTORS(UsbEventHandler);
};

UsbService::UsbService() {
  libusb_init(&context_);
  event_handler_ = new UsbEventHandler(context_);
}

UsbService::~UsbService() {}

void UsbService::Cleanup() {
  event_handler_->Stop();
  event_handler_ = NULL;
}

void UsbService::FindDevices(const uint16 vendor_id,
                             const uint16 product_id,
                             vector<scoped_refptr<UsbDevice> >* devices) {
  DCHECK(event_handler_) << "FindDevices called after event handler stopped.";
#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  if (!client)
    return;
  client->RequestUsbAccess(vendor_id,
                           product_id,
                           base::Bind(&UsbService::FindDevicesImpl,
                                      base::Unretained(this),
                                      vendor_id,
                                      product_id,
                                      devices));
#else
  FindDevicesImpl(vendor_id, product_id, devices, true);
#endif  // defined(OS_CHROMEOS)
}

void UsbService::FindDevicesImpl(const uint16 vendor_id,
                                 const uint16 product_id,
                                 vector<scoped_refptr<UsbDevice> >* devices,
                                 bool success) {
  devices->clear();

  // If the permission broker was unable to obtain permission for the specified
  // devices then there is no point in attempting to enumerate the devices. On
  // platforms without a permission broker, we assume permission is granted.
  if (!success)
    return;

  DeviceVector enumerated_devices;
  EnumerateDevices(&enumerated_devices);
  if (enumerated_devices.empty())
    return;

  for (unsigned int i = 0; i < enumerated_devices.size(); ++i) {
    PlatformUsbDevice device = enumerated_devices[i].device();
    if (DeviceMatches(device, vendor_id, product_id)) {
      UsbDevice* const wrapper = LookupOrCreateDevice(device);
      if (wrapper)
        devices->push_back(wrapper);
    }
  }
}

void UsbService::CloseDevice(scoped_refptr<UsbDevice> device) {
  DCHECK(event_handler_) << "CloseDevice called after event handler stopped.";

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

void UsbService::EnumerateDevices(DeviceVector* output) {
  STLClearObject(output);

  libusb_device** devices = NULL;
  const ssize_t device_count = libusb_get_device_list(context_, &devices);
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

UsbDevice* UsbService::LookupOrCreateDevice(PlatformUsbDevice device) {
  if (!ContainsKey(devices_, device)) {
    libusb_device_handle* handle = NULL;
    if (libusb_open(device, &handle)) {
      LOG(WARNING) << "Could not open device.";
      return NULL;
    }

    UsbDevice* wrapper = new UsbDevice(this, handle);
    devices_[device] = wrapper;
  }
  return devices_[device];
}
