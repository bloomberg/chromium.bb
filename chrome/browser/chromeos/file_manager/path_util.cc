// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/escape.h"

namespace file_manager {
namespace util {

namespace {

const char kDownloadsFolderName[] = "Downloads";
const base::FilePath::CharType kOldDownloadsFolderPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/Downloads");
const base::FilePath::CharType kOldDriveFolderPath[] =
    FILE_PATH_LITERAL("/special/drive");
// Unintended path introduced in crbug.com/363026.
const base::FilePath::CharType kBuggyDriveFolderPath[] =
    FILE_PATH_LITERAL("/special/drive-user");

}  // namespace

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // On non-ChromeOS system (test+development), the primary profile uses
  // $HOME/Downloads for ease for accessing local files for debugging.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      user_manager::UserManager::IsInitialized()) {
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            profile->GetOriginalProfile());
    const user_manager::User* const primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    if (user == primary_user)
      return DownloadPrefs::GetDefaultDownloadDirectory();
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

  std::vector<std::pair<base::FilePath, base::FilePath> > bases;
  bases.push_back(std::make_pair(base::FilePath(kOldDownloadsFolderPath),
                                 downloads));
  bases.push_back(std::make_pair(DownloadPrefs::GetDefaultDownloadDirectory(),
                                 downloads));
  bases.push_back(std::make_pair(base::FilePath(kOldDriveFolderPath), drive));
  bases.push_back(std::make_pair(base::FilePath(kBuggyDriveFolderPath), drive));

  // Trying migrating u-<hash>/Downloads to the current download path. This is
  // no-op when multi-profile is enabled. This is necessary for (1) back
  // migration when multi-profile flag is enabled and then disabled, or (2) in
  // some edge cases (crbug.com/356322) that u-<hash> path is temporarily used.
  if (user_manager::UserManager::IsInitialized()) {
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    if (user) {
      const base::FilePath hashed_downloads =
          chromeos::ProfileHelper::GetProfilePathByUserIdHash(
              user->username_hash()).AppendASCII(kDownloadsFolderName);
      bases.push_back(std::make_pair(hashed_downloads, downloads));
    }
  }

  for (size_t i = 0; i < bases.size(); ++i) {
    const base::FilePath& old_base = bases[i].first;
    const base::FilePath& new_base = bases[i].second;
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
  user_manager::User* const user =
      user_manager::UserManager::IsInitialized()
          ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : NULL;
  const std::string id = user ? "-" + user->username_hash() : "";
  return net::EscapeQueryParamValue(kDownloadsFolderName + id, false);
}

}  // namespace util
}  // namespace file_manager
