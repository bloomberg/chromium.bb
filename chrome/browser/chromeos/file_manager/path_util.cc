// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"

namespace file_manager {
namespace util {

const char kDownloadsFolderName[] = "Downloads";
const base::FilePath::CharType kOldDownloadsFolderPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/Downloads");

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // TODO(kinaba) crbug/309556: "Downloads" directory should be per-profile.
  //
  // For this to be per-profile, a unique directory path from profile->GetPath()
  // should be used rather than the HOME directory. We'll switch to the new path
  // once we have upgraded all code locations assuming the old path.
  base::FilePath path;
  CHECK(PathService::Get(base::DIR_HOME, &path));
  return path.AppendASCII(kDownloadsFolderName);
}

bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_path,
                              base::FilePath* new_path) {
  // /special/drive/xxx => /special/drive/root/xxx
  if (drive::util::NeedsNamespaceMigration(old_path)) {
    *new_path = drive::util::ConvertToMyDriveNamespace(old_path);
    return true;
  }

  // /home/chronos/user/Downloads/xxx => /home/chronos/u-hash/Downloads/xxx
  const base::FilePath old_base(kOldDownloadsFolderPath);
  base::FilePath relative;
  if (old_path == old_base ||
      old_base.AppendRelativePath(old_path, &relative)) {
    const base::FilePath new_base = GetDownloadsFolderForProfile(profile);
    *new_path = new_base.Append(relative);
    return old_path != *new_path;
  }

  return false;
}


}  // namespace util
}  // namespace file_manager
