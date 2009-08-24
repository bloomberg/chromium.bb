// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#import "base/base_paths.h"
#import "base/logging.h"
#import "base/path_service.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  bool success = false;
  NSArray* dirs =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                          NSUserDomainMask, YES);
  if ([dirs count] && result) {
    NSString* base = [dirs objectAtIndex:0];
#if defined(GOOGLE_CHROME_BUILD)
    base = [base stringByAppendingPathComponent:@"Google"];
    NSString* tail = @"Chrome";
#else
    NSString* tail = @"Chromium";
#endif
    NSString* path = [base stringByAppendingPathComponent:tail];
    *result = FilePath([path fileSystemRepresentation]);
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

FilePath GetBrowserBundlePath() {
  NSBundle* running_app_bundle = [NSBundle mainBundle];
  NSString* running_app_bundle_path = [running_app_bundle bundlePath];
  DCHECK(running_app_bundle_path) << "failed to get the main bundle path";

  // Are we the helper or the browser (main bundle)?
  if (![[[running_app_bundle infoDictionary]
           objectForKey:@"LSUIElement"] boolValue]) {
    // We aren't a LSUIElement, so this must be the browser, return it's path.
    return FilePath([running_app_bundle_path fileSystemRepresentation]);
  }

  // Helper lives at ...app/Contents/Resources/...Helper.app
  NSArray* components = [running_app_bundle_path pathComponents];
  DCHECK_GE([components count], static_cast<NSUInteger>(4))
      << "too few path components for this bundle to be within another bundle";
  components =
      [components subarrayWithRange:NSMakeRange(0, [components count] - 3)];

  NSString* browser_path = [NSString pathWithComponents:components];
  DCHECK([[browser_path pathExtension] isEqualToString:@"app"])
      << "we weren't within another app?";

  return FilePath([browser_path fileSystemRepresentation]);
}

}  // namespace chrome
