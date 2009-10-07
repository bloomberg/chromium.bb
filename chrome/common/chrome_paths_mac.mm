// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/path_service.h"

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

NSBundle* GetFrameworkBundle() {
  NSString* app_bundle_identifier = [[NSBundle mainBundle] bundleIdentifier];

  NSString* browser_bundle_identifier = app_bundle_identifier;
  if (mac_util::IsBackgroundOnlyProcess()) {
    // Take off the last component of a background helper process' bundle
    // identifier to form the browser process' bundle identifier.
    NSRange range = [app_bundle_identifier rangeOfString:@"."
                                                 options:NSBackwardsSearch];
    range.length = [app_bundle_identifier length] - range.location;
    browser_bundle_identifier =
        [app_bundle_identifier stringByReplacingCharactersInRange:range
                                                       withString:@""];
  }

  // Append ".framework" to the browser's bundle identifier to get the
  // framework's bundle identifier.
  NSString* framework_bundle_identifier =
      [browser_bundle_identifier stringByAppendingString:@".framework"];
  NSBundle* framework_bundle =
      [NSBundle bundleWithIdentifier:framework_bundle_identifier];

  return framework_bundle;
}

}  // namespace chrome
