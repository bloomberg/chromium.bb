// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOMediaBSDClient.h>
#include <paths.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/ucred.h>

#include <map>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"

namespace extensions {

namespace {

using api::experimental_system_info_storage::StorageUnitInfo;

// StorageInfoProvider implementation on MacOS platform.
class StorageInfoProviderMac : public StorageInfoProvider {
 public:
  StorageInfoProviderMac() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE;

 private:
  virtual ~StorageInfoProviderMac() {}
  void BuildStorageTypeMap();
  std::map<std::string, std::string> dev_path_to_type_map_;
};

bool StorageInfoProviderMac::QueryInfo(StorageInfo* info) {
  info->clear();

  struct statfs* mounted_volumes;
  int num_volumes = getmntinfo(&mounted_volumes, 0);
  if (num_volumes == 0)
    return false;

  BuildStorageTypeMap();

  for (int i = 0; i < num_volumes; ++i) {
    // Skip volumes which aren't displayed in the Finder
    if (mounted_volumes[i].f_flags & MNT_DONTBROWSE)
      continue;

    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    if (QueryUnitInfo(mounted_volumes[i].f_mntonname, unit.get()))
      info->push_back(unit);
  }
  return true;
}

bool StorageInfoProviderMac::QueryUnitInfo(const std::string& id,
                                           StorageUnitInfo* info) {
  struct statfs volume_info;
  if (statfs(id.c_str(), &volume_info) != 0)
    return false;

  std::string volume_dev(volume_info.f_mntfromname);
  int32 block_size = volume_info.f_bsize;
  info->id = id;
  std::string type = dev_path_to_type_map_[volume_dev];
  if (type.empty()) {
    BuildStorageTypeMap();
    type = dev_path_to_type_map_[volume_dev].empty() ?
        systeminfo::kStorageTypeUnknown : dev_path_to_type_map_[volume_dev];
  }
  info->type =
      api::experimental_system_info_storage::ParseStorageUnitType(type);
  // TODO(joshuagl): we're reporting different values than Disk Utility.
  // Is there an alternative API to get this information that doesn't use
  // statfs? NSFileManager's attributesOfFileSystemForPath uses statfs.
  info->capacity = volume_info.f_blocks * block_size;
  info->available_capacity = volume_info.f_bfree * block_size;
  return true;
}

void StorageInfoProviderMac::BuildStorageTypeMap() {
  io_iterator_t media_iterator;
  kern_return_t retval;

  retval = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                        IOServiceMatching(kIOMediaClass),
                                        &media_iterator);
  if (retval != KERN_SUCCESS)
    return;

  base::mac::ScopedIOObject<io_iterator_t> iterator(media_iterator);
  media_iterator = IO_OBJECT_NULL;

  for (base::mac::ScopedIOObject<io_service_t> media(IOIteratorNext(iterator));
       media;
       media.reset(IOIteratorNext(iterator))) {
    base::mac::ScopedCFTypeRef<CFTypeRef> dev_path_cf(
        IORegistryEntryCreateCFProperty(media,
                                        CFSTR(kIOBSDNameKey),
                                        kCFAllocatorDefault,
                                        0));

    if (!dev_path_cf)
      continue;

    std::string dev_path(_PATH_DEV);
    dev_path.append(
        base::SysCFStringRefToUTF8(
            base::mac::CFCast<CFStringRef>(dev_path_cf)));

    base::mac::ScopedCFTypeRef<CFTypeRef> removable_cf(
        IORegistryEntryCreateCFProperty(media,
                                        CFSTR(kIOMediaEjectableKey),
                                        kCFAllocatorDefault,
                                        0));
    if (!removable_cf)
      dev_path_to_type_map_[dev_path] = systeminfo::kStorageTypeUnknown;
    else if (CFBooleanGetValue(base::mac::CFCast<CFBooleanRef>(removable_cf)))
      dev_path_to_type_map_[dev_path] = systeminfo::kStorageTypeRemovable;
    else
      dev_path_to_type_map_[dev_path] =  systeminfo::kStorageTypeHardDisk;
  }
}

}  // namespace

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderMac>();
}

}  // namespace extensions
