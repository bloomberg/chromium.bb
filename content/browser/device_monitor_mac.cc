// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_monitor_mac.h"

#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/usb/IOUSBLib.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace content {

namespace {

const io_name_t kServices[] = {
  kIOFirstPublishNotification,
  kIOTerminatedNotification,
};

CFMutableDictionaryRef CreateMatchingDictionaryForUSBDevices(
    SInt32 interface_class_code, SInt32 interface_subclass_code) {
  CFMutableDictionaryRef matching_dictionary =
      IOServiceMatching(kIOUSBInterfaceClassName);
  base::mac::ScopedCFTypeRef<CFNumberRef> number_ref(CFNumberCreate(
      kCFAllocatorDefault, kCFNumberSInt32Type, &interface_class_code));
  DCHECK(number_ref);
  CFDictionaryAddValue(matching_dictionary, CFSTR(kUSBInterfaceClass),
                       number_ref);

  number_ref.reset(CFNumberCreate(kCFAllocatorDefault,
                                  kCFNumberSInt32Type,
                                  &interface_subclass_code));
  DCHECK(number_ref);
  CFDictionaryAddValue(matching_dictionary, CFSTR(kUSBInterfaceSubClass),
                       number_ref);

  return matching_dictionary;
}

void RegisterCallbackToIOService(IONotificationPortRef port,
                                 const io_name_t type,
                                 CFMutableDictionaryRef dictionary,
                                 IOServiceMatchingCallback callback,
                                 void* context,
                                 io_iterator_t* service) {
  kern_return_t err = IOServiceAddMatchingNotification(port,
                                                       type,
                                                       dictionary,
                                                       callback,
                                                       context,
                                                       service);
  if (err) {
    NOTREACHED() << "Failed to register the IO matched notification for type "
                 << type;
    return;
  }
  DCHECK(*service);

  // Iterate over set of matching devices to access already-present devices
  // and to arm the notification.
  for (base::mac::ScopedIOObject<io_service_t> object(IOIteratorNext(*service));
       object;
       object.reset(IOIteratorNext(*service))) {};
}

}  // namespace

DeviceMonitorMac::DeviceMonitorMac() {
  // Add the notification port to the run loop.
  notification_port_ = IONotificationPortCreate(kIOMasterPortDefault);
  DCHECK(notification_port_);

  RegisterAudioServices();
  RegisterVideoServices();

  CFRunLoopAddSource(CFRunLoopGetCurrent(),
                     IONotificationPortGetRunLoopSource(notification_port_),
                     kCFRunLoopCommonModes);
}

DeviceMonitorMac::~DeviceMonitorMac() {
  // Stop the notifications and free the objects.
  for (size_t i = 0; i < notification_iterators_.size(); ++i) {
    IOObjectRelease(*notification_iterators_[i]);
  }
  notification_iterators_.clear();

  // Remove the notification port from the message runloop.
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notification_port_),
                        kCFRunLoopCommonModes);

  // Destroy the notification port allocated by IONotificationPortCreate.
  IONotificationPortDestroy(notification_port_);
}

void DeviceMonitorMac::RegisterAudioServices() {
  CFMutableDictionaryRef dictionary =
      IOServiceMatching(kIOAudioDeviceClassName);
  RegisterServices(dictionary, &AudioDeviceCallback);
}

void DeviceMonitorMac::RegisterVideoServices() {
  CFMutableDictionaryRef dictionary = CreateMatchingDictionaryForUSBDevices(
      kUSBVideoInterfaceClass, kUSBVideoControlSubClass);
  RegisterServices(dictionary, &VideoDeviceCallback);
}

void DeviceMonitorMac::RegisterServices(CFMutableDictionaryRef dictionary,
                                        IOServiceMatchingCallback callback) {
  // Add callback to the service.
  for (size_t i = 0; i < arraysize(kServices); ++i) {
    // |dictionary| comes in with a reference count as 1. Since each call to
    // IOServiceAddMatchingNotification consumes one reference, we need to
    // retain |arraysize(kServices) -1| additional dictionary references.
    if (i < (arraysize(kServices) - 1))
      CFRetain(dictionary);

    // Register callback to each service.
    io_iterator_t service;
    RegisterCallbackToIOService(notification_port_,
                                kServices[i],
                                dictionary,
                                callback,
                                this,
                                &service);

    // Store the pointer of the object to release the memory when shutting
    // down the services.
    notification_iterators_.push_back(&service);
  }
}

void DeviceMonitorMac::AudioDeviceCallback(void *context,
                                           io_iterator_t iterator) {
 for (base::mac::ScopedIOObject<io_service_t> object(IOIteratorNext(iterator));
      object;
      object.reset(IOIteratorNext(iterator))) {
    if (context) {
      reinterpret_cast<DeviceMonitorMac*>(context)->NotifyDeviceChanged(
          base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);
    }
  }
}

void DeviceMonitorMac::VideoDeviceCallback(void *context,
                                           io_iterator_t iterator) {
  for (base::mac::ScopedIOObject<io_service_t> object(IOIteratorNext(iterator));
       object;
       object.reset(IOIteratorNext(iterator))) {
    if (context) {
      reinterpret_cast<DeviceMonitorMac*>(context)->NotifyDeviceChanged(
          base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    }
  }
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace content
