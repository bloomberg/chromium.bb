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
#include "net/base/escape.h"

namespace file_manager {
namespace util {

namespace {

const char kDownloadsFolderName[] = "Downloads";
const base::FilePath::CharType kOldDownloadsFolderPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/Downloads");
const base::FilePath::CharType kOldDriveFolderPath[] =
    FILE_PATH_LITERAL("/special/drive");
const base::FilePath::CharType kNoHashDriveFolderPath[] =
    FILE_PATH_LITERAL("/special/drive-");

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
  // /special/drive => /special/drive-xxx
  //
  // Old path format comes either from stored old settings or from the initial
  // default value set in DownloadPrefs::RegisterProfilePrefs in profile-unaware
  // code location. In the former case it is "/home/chronos/user/Downloads",
  // and in the latter case it is DownloadPrefs::GetDefaultDownloadDirectory().
  // Those two paths coincides as long as $HOME=/home/chronos/user, but the
  // environment variable is phasing out (crbug.com/333031) so we care both.

  const base::FilePath downloads = GetDownloadsFolderForProfile(profile);
  const base::FilePath drive = drive::util::GetDriveMountPointPath(profile);
  const base::FilePath bases[][2] = {
    {base::FilePath(kOldDownloadsFolderPath),      downloads},
    {DownloadPrefs::GetDefaultDownloadDirectory(), downloads},
    {base::FilePath(kOldDriveFolderPath),          drive},
    // TODO(kinaba): http://crbug.com/341284 Remove after M34 branching.
    // For a short period we incorrectly set "/special/drive-" as the Drive path
    // that needs to be fixed.
    {base::FilePath(kNoHashDriveFolderPath),       drive},
  };

  for (size_t i = 0; i < arraysize(bases); ++i) {
    const base::FilePath& old_base = bases[i][0];
    const base::FilePath& new_base = bases[i][1];
    base::FilePath relative;
    if (old_path == old_base ||
        old_base.AppendRelativePath(old_path, &relative)) {
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

}  // namespace util
}  // namespace file_manager
