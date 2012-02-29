// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mac/media_device_notifications.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "base/system_monitor/system_monitor.h"

namespace content {

namespace {

const CFStringRef kICAUserAssignedDeviceNameKey =
    CFSTR("ICAUserAssignedDeviceNameKey");
const CFStringRef kICDeviceNameKey = CFSTR("ifil");
const CFStringRef kICDeviceBSDNameKey = CFSTR("bsdName");

CFStringRef CopyMountPointFromBSDName(CFStringRef bsd_name) {
  base::mac::ScopedCFTypeRef<DASessionRef> session(
      DASessionCreate(kCFAllocatorDefault));
  if (!session.get())
    return NULL;

  base::mac::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(),
                              base::SysCFStringRefToUTF8(bsd_name).c_str()));
  if (!disk.get())
    return NULL;

  base::mac::ScopedCFTypeRef<CFDictionaryRef> description(
      DADiskCopyDescription(disk));
  if (!description.get())
    return NULL;

  CFURLRef mount_point = base::mac::GetValueFromDictionary<CFURLRef>(
      description, kDADiskDescriptionVolumePathKey);
  if (!mount_point)
    return NULL;

  return CFURLCopyFileSystemPath(mount_point, kCFURLPOSIXPathStyle);
}

bool GetDeviceInfo(unsigned long device_number, std::string* name,
                   FilePath* location) {
  ICACopyObjectPropertyDictionaryPB properties_request;
  properties_request.object = device_number;
  CFDictionaryRef device_properties;
  properties_request.theDict = &device_properties;
  if (ICACopyObjectPropertyDictionary(&properties_request, NULL) != noErr)
    return false;
  base::mac::ScopedCFTypeRef<CFDictionaryRef> scoped_device_properties(
      device_properties);

  CFStringRef device_name = base::mac::GetValueFromDictionary<CFStringRef>(
      device_properties, kICAUserAssignedDeviceNameKey);
  if (device_name == NULL) {
    device_name = base::mac::GetValueFromDictionary<CFStringRef>(
        device_properties, kICDeviceNameKey);
  }
  if (device_name == NULL)
    return false;
  *name = base::SysCFStringRefToUTF8(device_name);

  // TODO(vandebo) Support all media device, for now we only support mass
  // storage media devices.
  CFStringRef device = base::mac::GetValueFromDictionary<CFStringRef>(
      device_properties, kICDeviceBSDNameKey);
  if (device == NULL)
    return false;
  base::mac::ScopedCFTypeRef<CFStringRef>
      path(CopyMountPointFromBSDName(device));
  if (path.get() == NULL)
    return false;

  *location = FilePath(base::SysCFStringRefToUTF8(path).c_str());

  return true;
}

void MediaDeviceNotificationCallback(CFStringRef notification_type,
                                     CFDictionaryRef notification_dictionary) {
  bool attach = false;
  if (CFEqual(notification_type, kICANotificationTypeDeviceAdded)) {
    attach = true;
  } else if (!CFEqual(notification_type, kICANotificationTypeDeviceRemoved)) {
    return;
  }

  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();

  CFNumberRef device_number_object =
      base::mac::GetValueFromDictionary<CFNumberRef>(
          notification_dictionary, kICANotificationDeviceICAObjectKey);
  unsigned long device_number =
      [base::mac::CFToNSCast(device_number_object) unsignedLongValue];
  if (attach) {
    std::string device_name;
    FilePath location;
    if (GetDeviceInfo(device_number, &device_name, &location)) {
      system_monitor->ProcessMediaDeviceAttached(device_number, device_name,
                                                 location);
    }
  } else {
    system_monitor->ProcessMediaDeviceDetached(device_number);
  }
}

}  // namespace

void StartMediaDeviceNotifications() {
  NSArray* events_of_interest = [NSArray arrayWithObjects:
                                 (id)kICANotificationTypeDeviceAdded,
                                 (id)kICANotificationTypeDeviceRemoved,
                                 nil];

  ICARegisterForEventNotificationPB notification_request;
  notification_request.objectOfInterest = 0;  // Zero means all objects
  notification_request.eventsOfInterest =
      base::mac::NSToCFCast(events_of_interest);
  notification_request.notificationProc = &MediaDeviceNotificationCallback;
  notification_request.options = NULL;
  OSErr err = ICARegisterForEventNotification(&notification_request, NULL);
  CHECK_EQ(err, noErr);
}

}  // namespace content
