// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/hid/hid_connection_mac.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%04x", error_code);
}

bool TryGetHidIntProperty(IOHIDDeviceRef device,
                          CFStringRef key,
                          int32_t* result) {
  CFNumberRef ref =
      base::mac::CFCast<CFNumberRef>(IOHIDDeviceGetProperty(device, key));
  return ref && CFNumberGetValue(ref, kCFNumberSInt32Type, result);
}

int32_t GetHidIntProperty(IOHIDDeviceRef device, CFStringRef key) {
  int32_t value;
  if (TryGetHidIntProperty(device, key, &value))
    return value;
  return 0;
}

bool TryGetHidStringProperty(IOHIDDeviceRef device,
                             CFStringRef key,
                             std::string* result) {
  CFStringRef ref =
      base::mac::CFCast<CFStringRef>(IOHIDDeviceGetProperty(device, key));
  if (!ref) {
    return false;
  }
  *result = base::SysCFStringRefToUTF8(ref);
  return true;
}

std::string GetHidStringProperty(IOHIDDeviceRef device, CFStringRef key) {
  std::string value;
  TryGetHidStringProperty(device, key, &value);
  return value;
}

bool TryGetHidDataProperty(IOHIDDeviceRef device,
                           CFStringRef key,
                           std::vector<uint8>* result) {
  CFDataRef ref =
      base::mac::CFCast<CFDataRef>(IOHIDDeviceGetProperty(device, key));
  if (!ref) {
    return false;
  }
  STLClearObject(result);
  const uint8* bytes = CFDataGetBytePtr(ref);
  result->insert(result->begin(), bytes, bytes + CFDataGetLength(ref));
  return true;
}

}  // namespace

HidServiceMac::HidServiceMac(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner_.get());

  notify_port_ = IONotificationPortCreate(kIOMasterPortDefault);
  CFRunLoopAddSource(CFRunLoopGetMain(),
                     IONotificationPortGetRunLoopSource(notify_port_),
                     kCFRunLoopDefaultMode);

  io_iterator_t iterator;
  IOReturn result =
      IOServiceAddMatchingNotification(notify_port_,
                                       kIOFirstMatchNotification,
                                       IOServiceMatching(kIOHIDDeviceKey),
                                       FirstMatchCallback,
                                       this,
                                       &iterator);
  if (result != kIOReturnSuccess) {
    HID_LOG(ERROR) << "Failed to listen for device arrival: "
                   << HexErrorCode(result);
    return;
  }

  // Drain the iterator to arm the notification.
  devices_added_iterator_.reset(iterator);
  AddDevices();
  iterator = IO_OBJECT_NULL;

  result = IOServiceAddMatchingNotification(notify_port_,
                                            kIOTerminatedNotification,
                                            IOServiceMatching(kIOHIDDeviceKey),
                                            TerminatedCallback,
                                            this,
                                            &iterator);
  if (result != kIOReturnSuccess) {
    HID_LOG(ERROR) << "Failed to listen for device removal: "
                   << HexErrorCode(result);
    return;
  }

  // Drain devices_added_iterator_ to arm the notification.
  devices_removed_iterator_.reset(iterator);
  RemoveDevices();
  FirstEnumerationComplete();
}

void HidServiceMac::Connect(const HidDeviceId& device_id,
                            const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfo> device_info = map_entry->second;

  io_string_t service_path;
  strncpy(service_path, device_id.c_str(), sizeof service_path);
  base::mac::ScopedIOObject<io_service_t> service(
      IORegistryEntryFromPath(kIOMasterPortDefault, service_path));
  if (!service.get()) {
    HID_LOG(EVENT) << "IOService not found for path: " << device_id;
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service));
  if (!hid_device) {
    HID_LOG(EVENT) << "Unable to create IOHIDDevice object.";
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  IOReturn result = IOHIDDeviceOpen(hid_device, kIOHIDOptionsTypeNone);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to open device: " << HexErrorCode(result);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::Bind(callback, make_scoped_refptr(new HidConnectionMac(
                                          hid_device.release(), device_info,
                                          file_task_runner_))));
}

HidServiceMac::~HidServiceMac() {
}

// static
void HidServiceMac::FirstMatchCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_added_iterator_, iterator);
  service->AddDevices();
}

// static
void HidServiceMac::TerminatedCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_removed_iterator_, iterator);
  service->RemoveDevices();
}

void HidServiceMac::AddDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_service_t device;
  while ((device = IOIteratorNext(devices_added_iterator_)) != IO_OBJECT_NULL) {
    scoped_refptr<HidDeviceInfo> device_info = CreateDeviceInfo(device);
    if (device_info) {
      AddDevice(device_info);
      // The reference retained by IOIteratorNext is released below in
      // RemoveDevices when the device is removed.
    } else {
      IOObjectRelease(device);
    }
  }
}

void HidServiceMac::RemoveDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_service_t device;
  while ((device = IOIteratorNext(devices_removed_iterator_)) !=
         IO_OBJECT_NULL) {
    io_string_t service_path;
    IOReturn result =
        IORegistryEntryGetPath(device, kIOServicePlane, service_path);
    if (result == kIOReturnSuccess) {
      RemoveDevice(service_path);
    }

    // Release reference retained by AddDevices above.
    IOObjectRelease(device);
    // Release the reference retained by IOIteratorNext.
    IOObjectRelease(device);
  }
}

// static
scoped_refptr<HidDeviceInfo> HidServiceMac::CreateDeviceInfo(
    io_service_t service) {
  io_string_t service_path;
  IOReturn result =
      IORegistryEntryGetPath(service, kIOServicePlane, service_path);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to get IOService path: " << HexErrorCode(result);
    return nullptr;
  }

  base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service));
  if (!hid_device) {
    HID_LOG(EVENT) << "Unable to create IOHIDDevice object for " << service_path
                   << ".";
    return nullptr;
  }

  std::vector<uint8> report_descriptor;
  if (!TryGetHidDataProperty(hid_device, CFSTR(kIOHIDReportDescriptorKey),
                             &report_descriptor)) {
    HID_LOG(EVENT) << "Unable to get report descriptor for " << service_path
                   << ".";
    return nullptr;
  }

  return new HidDeviceInfo(
      service_path, GetHidIntProperty(hid_device, CFSTR(kIOHIDVendorIDKey)),
      GetHidIntProperty(hid_device, CFSTR(kIOHIDProductIDKey)),
      GetHidStringProperty(hid_device, CFSTR(kIOHIDProductKey)),
      GetHidStringProperty(hid_device, CFSTR(kIOHIDSerialNumberKey)),
      kHIDBusTypeUSB,  // TODO(reillyg): Detect Bluetooth. crbug.com/443335
      report_descriptor);
}

}  // namespace device
