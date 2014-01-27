// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "net/base/escape.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace file_manager {
namespace util {

namespace {

const char kDownloadsFolderName[] = "Downloads";
const base::FilePath::CharType kOldDownloadsFolderPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/Downloads");

}  // namespace

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !chromeos::UserManager::IsMultipleProfilesAllowed()) {
    // On the developer run on Linux desktop build, if multiple profiles are
    // not enabled, use $HOME/Downloads for ease for accessing local files for
    // debugging.
    base::FilePath path;
    CHECK(PathService::Get(base::DIR_HOME, &path));
    return path.AppendASCII(kDownloadsFolderName);
  }

  return profile->GetPath().AppendASCII(kDownloadsFolderName);
}

bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_path,
                              base::FilePath* new_path) {
  // M34:
  // /home/chronos/user/Downloads/xxx => /home/chronos/u-hash/Downloads/xxx
  //
  // Old path format comes either from stored old settings or from the initial
  // default value set in DownloadPrefs::RegisterProfilePrefs in profile-unaware
  // code location. In the former case it is "/home/chronos/user/Downloads",
  // and in the latter case it is DownloadPrefs::GetDefaultDownloadDirectory().
  // Those two paths coincides as long as $HOME=/home/chronos/user, but the
  // environment variable is phasing out (crbug.com/333031) so we care both.
  const base::FilePath old_bases[] = {
      base::FilePath(kOldDownloadsFolderPath),
      DownloadPrefs::GetDefaultDownloadDirectory(),
  };
  for (size_t i = 0; i < arraysize(old_bases); ++i) {
    const base::FilePath& old_base = old_bases[i];
    base::FilePath relative;
    if (old_path == old_base ||
        old_base.AppendRelativePath(old_path, &relative)) {
      const base::FilePath new_base = GetDownloadsFolderForProfile(profile);
      *new_path = new_base.Append(relative);
      return old_path != *new_path;
    }
  }

  return false;
}

std::string GetDownloadsMountPointName(Profile* profile) {
  // To distinguish profiles in multi-profile session, we append user name hash
  // to "Downloads". Note that some profiles (like login or test profiles)
  // are not associated with an user account. In that case, no suffix is added
  // because such a profile never belongs to a multi-profile session.
  chromeos::User* const user =
      chromeos::UserManager::IsInitialized() ?
          chromeos::UserManager::Get()->GetUserByProfile(
              profile->GetOriginalProfile()) : NULL;
  const std::string id = user ? "-" + user->username_hash() : "";
  return net::EscapePath(kDownloadsFolderName + id);
}

bool RegisterDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  // Although we show only profile's own "Downloads" folder in Files.app,
  // in the backend we need to mount all profile's download directory globally.
  // Otherwise, Files.app cannot support cross-profile file copies, etc.
  // For this reason, we need to register to the global GetSystemInstance().
  const std::string mount_point_name = GetDownloadsMountPointName(profile);
  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();

  // In some tests we want to override existing Downloads mount point, so we
  // first revoke the existing mount point (if any).
  mount_points->RevokeFileSystem(mount_point_name);
  return mount_points->RegisterFileSystem(
      mount_point_name, fileapi::kFileSystemTypeNativeLocal,
      fileapi::FileSystemMountOption(), path);
}

bool FindDownloadsMountPointPath(Profile* profile, base::FilePath* path) {
  const std::string mount_point_name = GetDownloadsMountPointName(profile);
  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();

  return mount_points->GetRegisteredPath(mount_point_name, path);
}

}  // namespace util
}  // namespace file_manager
