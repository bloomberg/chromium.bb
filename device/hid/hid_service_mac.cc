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
#include "device/hid/hid_connection_mac.h"

namespace device {

namespace {

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

void GetReportIds(IOHIDElementRef element, std::set<int>* reportIDs) {
  uint32_t reportID = IOHIDElementGetReportID(element);
  if (reportID) {
    reportIDs->insert(reportID);
  }

  CFArrayRef children = IOHIDElementGetChildren(element);
  if (!children) {
    return;
  }

  CFIndex childrenCount = CFArrayGetCount(children);
  for (CFIndex j = 0; j < childrenCount; ++j) {
    const IOHIDElementRef child = static_cast<IOHIDElementRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(children, j)));
    GetReportIds(child, reportIDs);
  }
}

void GetCollectionInfos(IOHIDDeviceRef device,
                        bool* has_report_id,
                        std::vector<HidCollectionInfo>* top_level_collections) {
  STLClearObject(top_level_collections);
  CFMutableDictionaryRef collections_filter =
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  const int kCollectionTypeValue = kIOHIDElementTypeCollection;
  CFNumberRef collection_type_id = CFNumberCreate(
      kCFAllocatorDefault, kCFNumberIntType, &kCollectionTypeValue);
  CFDictionarySetValue(
      collections_filter, CFSTR(kIOHIDElementTypeKey), collection_type_id);
  CFRelease(collection_type_id);
  CFArrayRef collections = IOHIDDeviceCopyMatchingElements(
      device, collections_filter, kIOHIDOptionsTypeNone);
  CFIndex collectionsCount = CFArrayGetCount(collections);
  *has_report_id = false;
  for (CFIndex i = 0; i < collectionsCount; i++) {
    const IOHIDElementRef collection = static_cast<IOHIDElementRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(collections, i)));
    // Top-Level Collection has no parent
    if (IOHIDElementGetParent(collection) == 0) {
      HidCollectionInfo collection_info;
      HidUsageAndPage::Page page = static_cast<HidUsageAndPage::Page>(
          IOHIDElementGetUsagePage(collection));
      uint16_t usage = IOHIDElementGetUsage(collection);
      collection_info.usage = HidUsageAndPage(usage, page);
      // Explore children recursively and retrieve their report IDs
      GetReportIds(collection, &collection_info.report_ids);
      if (collection_info.report_ids.size() > 0) {
        *has_report_id = true;
      }
      top_level_collections->push_back(collection_info);
    }
  }
}

void PopulateDeviceInfo(io_service_t service, HidDeviceInfo* device_info) {
  io_string_t service_path;
  IOReturn result =
      IORegistryEntryGetPath(service, kIOServicePlane, service_path);
  if (result != kIOReturnSuccess) {
    VLOG(1) << "Failed to get IOService path: "
            << base::StringPrintf("0x%04x", result);
    return;
  }

  base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service));
  device_info->device_id = service_path;
  device_info->vendor_id =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDVendorIDKey));
  device_info->product_id =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDProductIDKey));
  device_info->product_name =
      GetHidStringProperty(hid_device, CFSTR(kIOHIDProductKey));
  device_info->serial_number =
      GetHidStringProperty(hid_device, CFSTR(kIOHIDSerialNumberKey));
  GetCollectionInfos(
      hid_device, &device_info->has_report_id, &device_info->collections);
  device_info->max_input_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxInputReportSizeKey));
  if (device_info->has_report_id && device_info->max_input_report_size > 0) {
    device_info->max_input_report_size--;
  }
  device_info->max_output_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxOutputReportSizeKey));
  if (device_info->has_report_id && device_info->max_output_report_size > 0) {
    device_info->max_output_report_size--;
  }
  device_info->max_feature_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxFeatureReportSizeKey));
  if (device_info->has_report_id && device_info->max_feature_report_size > 0) {
    device_info->max_feature_report_size--;
  }
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
    LOG(ERROR) << "Failed to listen for device arrival: "
               << base::StringPrintf("0x%04x", result);
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
    LOG(ERROR) << "Failed to listen for device removal: "
               << base::StringPrintf("0x%04x", result);
    return;
  }

  // Drain devices_added_iterator_ to arm the notification.
  devices_removed_iterator_.reset(iterator);
  RemoveDevices();
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
    HidDeviceInfo device_info;
    PopulateDeviceInfo(device, &device_info);
    AddDevice(device_info);
    // The reference retained by IOIteratorNext is released below in
    // RemoveDevices when the device is removed.
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

scoped_refptr<HidConnection> HidServiceMac::Connect(
    const HidDeviceId& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    return NULL;
  }
  const HidDeviceInfo& device_info = map_entry->second;

  io_string_t service_path;
  strncpy(service_path, device_id.c_str(), sizeof service_path);
  base::mac::ScopedIOObject<io_service_t> service(
      IORegistryEntryFromPath(kIOMasterPortDefault, service_path));
  if (!service.get()) {
    VLOG(1) << "IOService not found for path: " << device_id;
    return NULL;
  }

  base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service));
  IOReturn result = IOHIDDeviceOpen(hid_device, kIOHIDOptionsTypeNone);
  if (result != kIOReturnSuccess) {
    VLOG(1) << "Failed to open device: "
            << base::StringPrintf("0x%04x", result);
    return NULL;
  }

  return make_scoped_refptr(new HidConnectionMac(
      hid_device.release(), device_info, file_task_runner_));
}

}  // namespace device
