// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

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

bool GetUserDocumentsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDownloadsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDesktop(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDesktopDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

FilePath GetVersionedDirectory() {
  static FilePath path;

  if (path.empty()) {
    // Start out with the path to the running .app.
    NSBundle* app_bundle = [NSBundle mainBundle];
    path = FilePath([[app_bundle bundlePath] fileSystemRepresentation]);

    if (mac_util::IsBackgroundOnlyProcess()) {
      // path identifies the helper .app in the browser .app's versioned
      // directory.  Go up one level to get to the browser .app's versioned
      // directory.
      path = path.DirName();
    } else {
      // path identifies the browser .app.  Go into its versioned directory.
      // TODO(mark): Here, |version| comes from the outer .app bundle's
      // Info.plist.  In the event of an incomplete update, it may be possible
      // for this value to be incorrect.  Consider the case where the updater
      // is able to update one, but not both, of the executable and the
      // Info.plist.  The executable may load one version of the framework,
      // and the Info.plist may refer to another version.  Ideally, the
      // version would be available in a compile-time constant, or there would
      // be a better way to detect the loaded framework version at runtime.
      NSString* version =
          [app_bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
      path = path.Append("Contents").
                  Append("Versions").
                  Append([version fileSystemRepresentation]);
    }
  }

  return path;
}

FilePath GetFrameworkBundlePath() {
  // It's tempting to use +[NSBundle bundleWithIdentifier:], but it's really
  // slow (about 30ms on 10.5 and 10.6), despite Apple's documentation stating
  // that it may be more efficient than +bundleForClass:.  +bundleForClass:
  // itself takes 1-2ms.  Getting an NSBundle from a path, on the other hand,
  // essentially takes no time at all, at least when the bundle has already
  // been loaded as it will have been in this case.  The FilePath operations
  // needed to compute the framework's path are also effectively free, so that
  // is the approach that is used here.

  // The framework bundle is at a known path and name from the browser .app's
  // versioned directory.
  return GetVersionedDirectory().Append(kFrameworkName);
}

}  // namespace chrome
