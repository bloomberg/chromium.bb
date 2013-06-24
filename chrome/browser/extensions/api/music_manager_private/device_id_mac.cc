// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DASession.h>
#include <DiskArbitration/DADisk.h>
#include <sys/mount.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kRootDirectory[] = "/";

// Return the BSD name (e.g. '/dev/disk1') of the root directory by enumerating
// through the mounted volumes .
// Return "" if an error occured.
std::string FindBSDNameOfSystemDisk() {
  struct statfs* mounted_volumes;
  int num_volumes = getmntinfo(&mounted_volumes, 0);
  if (num_volumes == 0) {
    VLOG(1) << "Cannot enumerate list of mounted volumes.";
    return std::string();
  }

  for (int i = 0; i < num_volumes; i++) {
    struct statfs* vol = &mounted_volumes[i];
    if (std::string(vol->f_mntonname) == kRootDirectory) {
      return std::string(vol->f_mntfromname);
    }
  }

  VLOG(1) << "Cannot find disk mounted as '" << kRootDirectory << "'.";
  return std::string();
}

// Return the Volume UUID property of a BSD disk name (e.g. '/dev/disk1').
// Return "" if an error occured.
std::string GetVolumeUUIDFromBSDName(const std::string& bsd_name) {
  const CFAllocatorRef allocator = NULL;

  base::ScopedCFTypeRef<DASessionRef> session(DASessionCreate(allocator));
  if (session.get() == NULL) {
    VLOG(1) << "Error creating DA Session.";
    return std::string();
  }

  base::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(allocator, session, bsd_name.c_str()));
  if (disk.get() == NULL) {
    VLOG(1) << "Error creating DA disk from BSD disk name.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFDictionaryRef> disk_description(
      DADiskCopyDescription(disk));
  if (disk_description.get() == NULL) {
    VLOG(1) << "Error getting disk description.";
    return std::string();
  }

  CFUUIDRef volume_uuid = base::mac::GetValueFromDictionary<CFUUIDRef>(
      disk_description,
      kDADiskDescriptionVolumeUUIDKey);
  if (volume_uuid == NULL) {
    VLOG(1) << "Error getting volume UUID of disk.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFStringRef> volume_uuid_string(
      CFUUIDCreateString(allocator, volume_uuid));
  if (volume_uuid_string.get() == NULL) {
    VLOG(1) << "Error creating string from CSStringRef.";
    return std::string();
  }

  return base::SysCFStringRefToUTF8(volume_uuid_string.get());
}

// Return Volume UUID property of disk mounted as "/".
void GetVolumeUUID(const extensions::api::DeviceId::IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string result;
  std::string bsd_name = FindBSDNameOfSystemDisk();
  if (!bsd_name.empty()) {
    VLOG(4) << "BSD name of root directory: '" << bsd_name << "'";
    result = GetVolumeUUIDFromBSDName(bsd_name);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

}

namespace extensions {
namespace api {

// MacOS: Return Volume UUID property of disk mounted as "/".
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(GetVolumeUUID, callback));
}

}  // namespace api
}  // namespace extensions
