// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace file_manager {
namespace util {

const char kDownloadsFolderName[] = "Downloads";
const base::FilePath::CharType kOldDownloadsFolderPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/Downloads");

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    // On the developer run on Linux desktop build, user $HOME/Downloads
    // for ease for accessing local files for debugging.
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
  // TODO(kinaba): crbug.com/336090.
  // Make it unique for each profile once Files.app is ready.
  return kDownloadsFolderName;
}

bool RegisterDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  // TODO(kinaba): crbug.com/336090.
  // Register to fileapi::ExternalMountPoint::GetSystemInstance rather than the
  // per-profile instance, because it needs to be reachable from every profile
  // for enabling cross-profile copies, etc.

  const std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  fileapi::ExternalMountPoints* const mount_points =
      content::BrowserContext::GetMountPoints(profile);

  // The Downloads mount point already exists so it must be removed before
  // adding the test mount point (which will also be mapped as Downloads).
  mount_points->RevokeFileSystem(mount_point_name);
  return mount_points->RegisterFileSystem(
      mount_point_name, fileapi::kFileSystemTypeNativeLocal,
      fileapi::FileSystemMountOption(), path);
}

}  // namespace util
}  // namespace file_manager
