// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include <windows.h>

namespace extensions {

namespace {

using api::experimental_system_info_storage::StorageUnitInfo;

// Logical drive string length is 4. It contains one driver letter character,
// L":", L"\" and a terminating null character.
const unsigned long kMaxLogicalDriveString = 4 * 26;

// StorageInfoProvider implementation on Windows platform.
class StorageInfoProviderWin : public StorageInfoProvider {
 public:
  StorageInfoProviderWin() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE;
 private:
  virtual ~StorageInfoProviderWin() {}
};

bool StorageInfoProviderWin::QueryInfo(StorageInfo* info) {
  info->clear();

  WCHAR logical_drive_strings[kMaxLogicalDriveString];

  // The total length of drive string returned from GetLogicalDriveStrings
  // must be divided exactly by 4 since each drive is represented by 4 wchars.
  DWORD string_length = GetLogicalDriveStrings(
      kMaxLogicalDriveString - 1, logical_drive_strings);
  DCHECK(string_length % 4 == 0);
  // No drive found, return false.
  if (string_length == 0)
    return false;

  // Iterate the drive string by 4 wchars each step
  for (unsigned int i = 0; i < string_length; i += 4) {
    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    if (QueryUnitInfo(base::WideToUTF8(&logical_drive_strings[i]),
                      unit.get())) {
      info->push_back(unit);
    }
  }
  return true;
}

bool StorageInfoProviderWin::QueryUnitInfo(const std::string& id,
                                           StorageUnitInfo* info) {
  DCHECK(info);
  string16 drive = UTF8ToUTF16(id);

  std::string type;
  DWORD ret = GetDriveType(drive.c_str());
  switch (ret) {
    case DRIVE_FIXED:
      type = systeminfo::kStorageTypeHardDisk;
      break;
    case DRIVE_REMOVABLE:
      type = systeminfo::kStorageTypeRemovable;
      break;
    case DRIVE_UNKNOWN:
      type = systeminfo::kStorageTypeUnknown;
      break;
    case DRIVE_CDROM:       // CD ROM
    case DRIVE_REMOTE:      // Remote network drive
    case DRIVE_NO_ROOT_DIR: // Invalid root path
    case DRIVE_RAMDISK:     // RAM disk
      // TODO(hmin): Do we need to care about these drive types?
      return false;
  }

  ULARGE_INTEGER total_bytes;
  ULARGE_INTEGER free_bytes;

  if (GetDiskFreeSpaceEx(drive.c_str(), NULL, &total_bytes, &free_bytes)) {
    info->id = id;
    info->type =
        api::experimental_system_info_storage::ParseStorageUnitType(type);
    info->capacity = static_cast<double>(total_bytes.QuadPart);
    info->available_capacity = static_cast<double>(free_bytes.QuadPart);
    return true;
  }
  return false;
}

}  // namespace

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderWin>();
}

}  // namespace extensions
