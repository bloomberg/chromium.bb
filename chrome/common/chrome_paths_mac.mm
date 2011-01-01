// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

namespace {
const FilePath* g_override_versioned_directory = NULL;
}  // namespace

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  bool success = false;
  if (result && PathService::Get(base::DIR_APP_DATA, result)) {
#if defined(GOOGLE_CHROME_BUILD)
    *result = result->Append("Google").Append("Chrome");
#else
    *result = result->Append("Chromium");
#endif
    success = true;
  }
  return success;
}

bool GetChromeFrameUserDataDirectory(FilePath* result) {
  bool success = false;
  if (result && PathService::Get(base::DIR_APP_DATA, result)) {
#if defined(GOOGLE_CHROME_BUILD)
    *result = result->Append("Google").Append("Chrome Frame");
#else
    *result = result->Append("Chrome Frame");
#endif
    success = true;
  }
  return success;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  return base::mac::GetUserDirectory(NSDocumentDirectory, result);
}

void GetUserCacheDirectory(const FilePath& profile_dir, FilePath* result) {
  // If the profile directory is under ~/Library/Application Support,
  // use a suitable cache directory under ~/Library/Caches.  For
  // example, a profile directory of ~/Library/Application
  // Support/Google/Chrome/MyProfileName would use the cache directory
  // ~/Library/Caches/Google/Chrome/MyProfileName.

  // Default value in cases where any of the following fails.
  *result = profile_dir;

  FilePath app_data_dir;
  if (!PathService::Get(base::DIR_APP_DATA, &app_data_dir))
    return;
  FilePath cache_dir;
  if (!PathService::Get(base::DIR_CACHE, &cache_dir))
    return;
  if (!app_data_dir.AppendRelativePath(profile_dir, &cache_dir))
    return;

  *result = cache_dir;
}

bool GetUserDownloadsDirectory(FilePath* result) {
  return base::mac::GetUserDirectory(NSDownloadsDirectory, result);
}

bool GetUserDesktop(FilePath* result) {
  return base::mac::GetUserDirectory(NSDesktopDirectory, result);
}

FilePath GetVersionedDirectory() {
  if (g_override_versioned_directory)
    return *g_override_versioned_directory;

  // Start out with the path to the running executable.
  FilePath path;
  PathService::Get(base::FILE_EXE, &path);

  // One step up to MacOS, another to Contents.
  path = path.DirName().DirName();
  DCHECK_EQ(path.BaseName().value(), "Contents");

  if (base::mac::IsBackgroundOnlyProcess()) {
    // path identifies the helper .app's Contents directory in the browser
    // .app's versioned directory.  Go up two steps to get to the browser
    // .app's versioned directory.
    path = path.DirName().DirName();
    DCHECK_EQ(path.BaseName().value(), kChromeVersion);
  } else {
    // Go into the versioned directory.
    path = path.Append("Versions").Append(kChromeVersion);
  }

  return path;
}

void SetOverrideVersionedDirectory(const FilePath* path) {
  if (path != g_override_versioned_directory) {
    delete g_override_versioned_directory;
    g_override_versioned_directory = path;
  }
}

FilePath GetFrameworkBundlePath() {
  // It's tempting to use +[NSBundle bundleWithIdentifier:], but it's really
  // slow (about 30ms on 10.5 and 10.6), despite Apple's documentation stating
  // that it may be more efficient than +bundleForClass:.  +bundleForClass:
  // itself takes 1-2ms.  Getting an NSBundle from a path, on the other hand,
  // essentially takes no time at all, at least when the bundle has already
  // been loaded as it will have been in this case.  The FilePath operations
  // needed to compute the framework's path are also effectively free, so that
  // is the approach that is used here.  NSBundle is also documented as being
  // not thread-safe, and thread safety may be a concern here.

  // The framework bundle is at a known path and name from the browser .app's
  // versioned directory.
  return GetVersionedDirectory().Append(kFrameworkName);
}

bool GetLocalLibraryDirectory(FilePath* result) {
  return base::mac::GetLocalDirectory(NSLibraryDirectory, result);
}

}  // namespace chrome
