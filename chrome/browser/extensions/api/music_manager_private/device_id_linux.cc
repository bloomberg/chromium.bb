// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include <map>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kDiskByUuidDirectoryName[] = "/dev/disk/by-uuid";
const char* kDeviceNames[] = { "sda1", "hda1", "dm-0" };

// Map from device name to disk uuid
typedef std::map<base::FilePath, base::FilePath> DiskEntries;

void GetDiskUuid(const extensions::api::DeviceId::IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  DiskEntries disk_uuids;

  base::FileEnumerator files(base::FilePath(kDiskByUuidDirectoryName),
                             false,  // Recursive.
                             base::FileEnumerator::FILES);
  do {
    base::FilePath file_path = files.Next();
    if (file_path.empty())
      break;

    base::FilePath target_path;
    if (!file_util::ReadSymbolicLink(file_path, &target_path))
      continue;

    base::FilePath device_name = target_path.BaseName();
    base::FilePath disk_uuid = file_path.BaseName();
    disk_uuids[device_name] = disk_uuid;
  } while (true);

  // Look for first device name matching an entry of |kDeviceNames|.
  std::string result;
  for (size_t i = 0; i < arraysize(kDeviceNames); i++) {
    DiskEntries::iterator it =
        disk_uuids.find(base::FilePath(kDeviceNames[i]));
    if (it != disk_uuids.end()) {
      DVLOG(1) << "Returning uuid: \"" << it->second.value()
               << "\" for device \"" << it->first.value() << "\"";
      result = it->second.value();
      break;
    }
  }

  // Log failure (at most once) for diagnostic purposes.
  static bool error_logged = false;
  if (result.empty() && !error_logged) {
    error_logged = true;
    LOG(ERROR) << "Could not find appropriate disk uuid.";
    for (DiskEntries::iterator it = disk_uuids.begin();
        it != disk_uuids.end(); ++it) {
      LOG(ERROR) << "  DeviceID=" << it->first.value() << ", uuid="
                 << it->second.value();
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

}  // namespace

namespace extensions {
namespace api {

// Linux: Look for disk uuid
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(GetDiskUuid, callback));
}

}  // namespace api
}  // namespace extensions
