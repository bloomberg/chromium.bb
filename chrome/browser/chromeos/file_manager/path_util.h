// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "storage/browser/fileapi/file_system_url.h"

class GURL;
class Profile;

namespace file_manager {
namespace util {

// Absolute base path for removable media on Chrome OS. Exposed here so it can
// be used by tests.
extern const base::FilePath::CharType kRemovableMediaPath[];

// Gets the absolute path for the 'Downloads' folder for the |profile|.
base::FilePath GetDownloadsFolderForProfile(Profile* profile);

// Converts |old_path| to |new_path| and returns true, if the old path points
// to an old location of user folders (in "Downloads" or "Google Drive").
// The |profile| argument is used for determining the location of the
// "Downloads" folder.
//
// As of now (M40), the conversion is used only during initialization of
// download_prefs, where profile unaware initialization precedes profile
// aware stage. Below are the list of relocations we have made in the past.
//
// M27: crbug.com/229304, for supporting {offline, recent, shared} folders
//   in Drive. Migration code for this is removed in M34.
// M34-35: crbug.com/313539, 356322, for supporting multi profiles.
//   Migration code is removed in M40.
bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_path,
                              base::FilePath* new_path);

// The canonical mount point name for "Downloads" folder.
std::string GetDownloadsMountPointName(Profile* profile);

// DEPRECATED. Use |ConvertToContentUrl| instead.
// While this function can convert paths under Downloads, /media/removable
// and /special/drive, this CANNOT convert paths under ARC media directories
// (/special/arc-documents-provider).
// TODO(crbug.com/811679): Migrate all callers and remove this.
bool ConvertPathToArcUrl(const base::FilePath& path, GURL* arc_url_out);

using ConvertToContentUrlCallback =
    base::OnceCallback<void(const GURL& content_url)>;

// Asynchronously converts a Chrome OS file system URL to a content:// URL.
// Returns an empty GURL if |file_system_url| is not convertible.
void ConvertToContentUrl(const storage::FileSystemURL& file_system_url,
                         ConvertToContentUrlCallback callback);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
