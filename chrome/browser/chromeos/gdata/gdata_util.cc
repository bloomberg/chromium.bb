// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/escape.h"

namespace gdata {
namespace util {

namespace {

const char kGDataSpecialRootPath[] = "/special";

const char kGDataMountPointPath[] = "/special/gdata";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "gdata"
};

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

}  // namespace

const FilePath& GetGDataMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataMountPointPath)));
  return gdata_mount_path;
}

const std::string& GetGDataMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, gdata_mount_path_string,
      (kGDataMountPointPath));
  return gdata_mount_path_string;
}

const FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataSpecialRootPath)));
  return gdata_mount_path;
}

GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name) {
  return GURL(base::StringPrintf(
      "chrome://gdata/%s/%s",
      net::EscapePath(resource_id).c_str(),
      net::EscapePath(file_name).c_str()));
}

bool IsUnderGDataMountPoint(const FilePath& path) {
  return GetGDataMountPointPath() == path ||
         GetGDataMountPointPath().IsParent(path);
}

FilePath ExtractGDataPath(const FilePath& path) {
  if (!IsUnderGDataMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'gdata'.
  FilePath extracted;
  for (size_t i = arraysize(kGDataMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}


void SetPermissionsForGDataCacheFiles(Profile* profile,
                                      int pid,
                                      const FilePath& path) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  if (!system_service || !system_service->file_system())
    return;

  GDataFileSystem* file_system = system_service->file_system();

  GDataFileProperties file_properties;
  file_system->GetFileInfoFromPath(path, &file_properties);

  std::string resource_id = file_properties.resource_id;
  std::string file_md5 = file_properties.file_md5;

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for gdata/ paths.
  std::vector<std::pair<FilePath, int> > cache_paths;
  cache_paths.push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));
  // TODO(tbarzic): When we start supporting openFile operation, we may have to
  // change permission for localy modified files to match handler's permissions.
  cache_paths.push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED),
     kReadOnlyFilePermissions));
  cache_paths.push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));

  for (size_t i = 0; i < cache_paths.size(); i++) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        pid, cache_paths[i].first, cache_paths[i].second);
  }
}

bool IsGDataAvailable(Profile* profile) {
  // We allow GData only in canary and dev channels.  http://crosbug.com/28806
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_BETA ||
      channel == chrome::VersionInfo::CHANNEL_STABLE)
    return false;

  // Do not allow GData for incognito windows / guest mode.
  if (profile->IsOffTheRecord())
    return false;

  // Disable gdata if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return false;

  return true;
}

}  // namespace util
}  // namespace gdata
