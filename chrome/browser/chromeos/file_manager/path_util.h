// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_

#include <string>

class Profile;

namespace base {
class FilePath;
}

namespace file_manager {
namespace util {

// Gets the absolute path for the 'Downloads' folder for the |profile|.
base::FilePath GetDownloadsFolderForProfile(Profile* profile);

// Converts |old_path| to |new_path| and returns true, if the old path points
// to an old location of user folders (in "Downloads" or "Google Drive").
// The |profile| argument is used for determining the location of the
// "Downloads" folder.
//
// Here's the list of relocations we have made so far.
//
// M27: crbug.com/229304, Migration code for this is removed in M34.
//   The "Google Drive" folder is moved from /special/drive to
//   /special/drive/root to stored shared files outside of "My Drive" in
//   /special/drive/other.
//
// M34: crbug.com/313539
//   The "Downloads" folder is changed from /home/chronos/user/Downloads to
//   /home/chronos/u-<hash>/Downloads when multi-profile is enabled.
//
//   The path "/home/chronos/user" is a hard link to the u-<hash> directory of
//   the primary profile of the current session. The two paths always meant the
//   same directory before multi-profiles. However, for secondary profiles in
//   a multi-profile session, the "user" path cannot be used to mean "its own"
//   Download folder anymore. Thus we are switching to always use "u-<hash>"
//   that consistently works whether or not it is primary.
//
// M35: crbug.com/356322
//   It turned out even if multi-profile is disabled, u-<hash> style profile
//   can be used in some situations. To address the cases, we add a migration
//   from /home/chronos/u-<hash>/Downloads to current Download path.
//   This just results in no-op when multi-profile is enabled.
bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_path,
                              base::FilePath* new_path);

// The canonical mount point name for "Downloads" folder.
std::string GetDownloadsMountPointName(Profile* profile);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
