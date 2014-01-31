// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_mac.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_connection_mac.h"
#include "device/hid/hid_utils_mac.h"
#include "net/base/io_buffer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

namespace device {

class HidServiceMac;

HidServiceMac::HidServiceMac() : enumeration_runloop_init_(true, false) {
  base::ThreadRestrictions::AssertIOAllowed();

  hid_manager_ref_.reset(IOHIDManagerCreate(kCFAllocatorDefault,
                                            kIOHIDOptionsTypeNone));
  if (!hid_manager_ref_ ||
      CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    LOG(ERROR) << "Failed to initialize HidManager";
    return;
  }
  CFRetain(hid_manager_ref_);

  // Register for plug/unplug notifications.
  IOHIDManagerSetDeviceMatching(hid_manager_ref_.get(), NULL);
  IOHIDManagerRegisterDeviceMatchingCallback(
      hid_manager_ref_.get(),
      &HidServiceMac::AddDeviceCallback,
      this);
  IOHIDManagerRegisterDeviceRemovalCallback(
      hid_manager_ref_.get(),
      &HidServiceMac::RemoveDeviceCallback,
      this);

  // Blocking operation to enumerate all the pre-existing devices.
  Enumerate();

  // Save the owner message loop.
  message_loop_ = base::MessageLoopProxy::current();

  // Start a thread to monitor HID device changes.
  // The reason to create a new thread is that by default the only thread in the
  // browser process having a CFRunLoop is the UI thread. We do not want to
  // run potentially blocking routines on it.
  enumeration_runloop_thread_.reset(
      new base::Thread("HidService Device Enumeration Thread"));

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_UI;
  enumeration_runloop_thread_->StartWithOptions(options);
  enumeration_runloop_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceMac::ScheduleRunLoop, base::Unretained(this)));

  enumeration_runloop_init_.Wait();
  initialized_ = true;
}

HidServiceMac::~HidServiceMac() {
  enumeration_runloop_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&HidServiceMac::UnscheduleRunLoop, base::Unretained(this)));

  enumeration_runloop_thread_->Stop();
}

void HidServiceMac::ScheduleRunLoop() {
  enumeration_runloop_ = CFRunLoopGetCurrent();

  IOHIDManagerScheduleWithRunLoop(
      hid_manager_ref_,
      enumeration_runloop_,
      kCFRunLoopDefaultMode);

  IOHIDManagerOpen(hid_manager_ref_, kIOHIDOptionsTypeNone);

  enumeration_runloop_init_.Signal();
}

void HidServiceMac::UnscheduleRunLoop() {
  IOHIDManagerUnscheduleFromRunLoop(
        hid_manager_ref_,
        enumeration_runloop_,
        kCFRunLoopDefaultMode);

  IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);

}

HidServiceMac* HidServiceMac::InstanceFromContext(void* context) {
  return reinterpret_cast<HidServiceMac*>(context);
}

void HidServiceMac::AddDeviceCallback(void* context,
                                      IOReturn result,
                                      void* sender,
                                      IOHIDDeviceRef ref) {
  HidServiceMac* service = InstanceFromContext(context);

  // Takes ownership of ref. Will be released inside PlatformDeviceAdd.
  CFRetain(ref);
  service->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceMac::PlatformAddDevice,
                 base::Unretained(service),
                 base::Unretained(ref)));
}

void HidServiceMac::RemoveDeviceCallback(void* context,
                                         IOReturn result,
                                         void* sender,
                                         IOHIDDeviceRef ref) {
  HidServiceMac* service = InstanceFromContext(context);

  // Takes ownership of ref. Will be released inside PlatformDeviceRemove.
  CFRetain(ref);
  service->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceMac::PlatformRemoveDevice,
                 base::Unretained(service),
                 base::Unretained(ref)));
}

IOHIDDeviceRef HidServiceMac::FindDevice(std::string id) {
  base::ScopedCFTypeRef<CFSetRef> devices(
      IOHIDManagerCopyDevices(hid_manager_ref_));
  CFIndex count = CFSetGetCount(devices);
  scoped_ptr<IOHIDDeviceRef[]> device_refs(new IOHIDDeviceRef[count]);
  CFSetGetValues(devices, (const void **)(device_refs.get()));

  for (CFIndex i = 0; i < count; i++) {
    int32_t int_property = 0;
    if (GetHidIntProperty(device_refs[i], CFSTR(kIOHIDLocationIDKey),
                       &int_property)) {
      if (id == base::HexEncode(&int_property, sizeof(int_property))) {
        return device_refs[i];
      }
    }
  }

  return NULL;
}

void HidServiceMac::Enumerate() {
  base::ScopedCFTypeRef<CFSetRef> devices(
      IOHIDManagerCopyDevices(hid_manager_ref_));
  CFIndex count = CFSetGetCount(devices);
  scoped_ptr<IOHIDDeviceRef[]> device_refs(new IOHIDDeviceRef[count]);
  CFSetGetValues(devices, (const void **)(device_refs.get()));

  for (CFIndex i = 0; i < count; i++) {
    // Takes ownership. Will be released inside PlatformDeviceAdd.
    CFRetain(device_refs[i]);
    PlatformAddDevice(device_refs[i]);
  }
}

void HidServiceMac::PlatformAddDevice(IOHIDDeviceRef raw_ref) {
  HidDeviceInfo device;
  int32_t int_property = 0;
  std::string str_property;

  // Auto-release.
  base::ScopedCFTypeRef<IOHIDDeviceRef> ref(raw_ref);

  // Unique identifier for HID device.
  if (GetHidIntProperty(ref, CFSTR(kIOHIDLocationIDKey), &int_property)) {
    device.device_id = base::HexEncode(&int_property, sizeof(int_property));
  } else {
    // Not an available device.
    return;
  }

  if (GetHidIntProperty(ref, CFSTR(kIOHIDVendorIDKey), &int_property)) {
    device.vendor_id = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDProductIDKey), &int_property)) {
    device.product_id = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDPrimaryUsageKey), &int_property)) {
    device.usage = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDPrimaryUsagePageKey), &int_property)) {
    device.usage_page = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDMaxInputReportSizeKey),
                        &int_property)) {
    device.input_report_size = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDMaxOutputReportSizeKey),
                        &int_property)) {
    device.output_report_size = int_property;
  }
  if (GetHidIntProperty(ref, CFSTR(kIOHIDMaxFeatureReportSizeKey),
                     &int_property)) {
    device.feature_report_size = int_property;
  }
  if (GetHidStringProperty(ref, CFSTR(kIOHIDProductKey), &str_property)) {
    device.product_name = str_property;
  }
  if (GetHidStringProperty(ref, CFSTR(kIOHIDSerialNumberKey), &str_property)) {
    device.serial_number = str_property;
  }
  HidService::AddDevice(device);
}

void HidServiceMac::PlatformRemoveDevice(IOHIDDeviceRef raw_ref) {
  std::string device_id;
  int32_t int_property = 0;
  // Auto-release.
  base::ScopedCFTypeRef<IOHIDDeviceRef> ref(raw_ref);
  if (!GetHidIntProperty(ref, CFSTR(kIOHIDLocationIDKey), &int_property)) {
    return;
  }
  device_id = base::HexEncode(&int_property, sizeof(int_property));
  HidService::RemoveDevice(device_id);
}

scoped_refptr<HidConnection>
HidServiceMac::Connect(std::string device_id) {
  if (!ContainsKey(devices_, device_id))
    return NULL;

  IOHIDDeviceRef ref = FindDevice(device_id);
  if (ref == NULL)
    return NULL;
  return scoped_refptr<HidConnection>(
      new HidConnectionMac(this, devices_[device_id], ref));
}

}  // namespace device
