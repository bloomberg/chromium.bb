// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/app_mode_chrome_locator.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"

namespace app_mode {

bool FindBundleById(NSString* bundle_id, base::FilePath* out_bundle) {
  NSWorkspace* ws = [NSWorkspace sharedWorkspace];
  NSString *bundlePath = [ws absolutePathForAppBundleWithIdentifier:bundle_id];
  if (!bundlePath)
    return false;

  *out_bundle = base::mac::NSStringToFilePath(bundlePath);
  return true;
}

bool GetChromeBundleInfo(const base::FilePath& chrome_bundle,
                         string16* raw_version_str,
                         base::FilePath* version_path,
                         base::FilePath* framework_shlib_path) {
  using base::mac::ObjCCast;

  NSString* cr_bundle_path = base::mac::FilePathToNSString(chrome_bundle);
  NSBundle* cr_bundle = [NSBundle bundleWithPath:cr_bundle_path];

  if (!cr_bundle)
    return false;

  // Read raw version string.
  NSString* cr_version =
       ObjCCast<NSString>(
          [cr_bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
  if (!cr_version)
    return false;

  // Get versioned directory.
  NSArray* cr_versioned_path_components =
      [NSArray arrayWithObjects:cr_bundle_path,
                                @"Contents",
                                @"Versions",
                                cr_version,
                                nil];
  NSString* cr_versioned_path =
    [NSString pathWithComponents:cr_versioned_path_components];

  // Get the framework path.
  NSString* cr_bundle_exe =
      ObjCCast<NSString>(
          [cr_bundle objectForInfoDictionaryKey:@"CFBundleExecutable"]);
  NSString* cr_framework_shlib_path =
      [cr_versioned_path stringByAppendingPathComponent:
          base::SysUTF8ToNSString(chrome::kFrameworkName)];
  // chrome::kFrameworkName looks like "$PRODUCT_STRING Framework.framework".
  // The library itself is at
  // "$PRODUCT_STRING Framework.framework/$PRODUCT_STRING Framework", so we cut
  // off the .framework extension here and append it to the path.
  // It's important to build the path to the framework this way because
  // in Canary the framework is still called "Google Chrome Framework".
  cr_framework_shlib_path =
      [cr_framework_shlib_path stringByAppendingPathComponent:
          [base::SysUTF8ToNSString(chrome::kFrameworkName)
              stringByDeletingPathExtension]];
  if (!cr_bundle_exe || !cr_framework_shlib_path)
    return false;

  // A few more sanity checks.
  BOOL is_directory;
  BOOL exists = [[NSFileManager defaultManager]
                    fileExistsAtPath:cr_framework_shlib_path
                         isDirectory:&is_directory];
  if (!exists || is_directory)
    return false;

  // Everything OK, copy output parameters.
  *raw_version_str = base::SysNSStringToUTF16(cr_version);
  *version_path = base::mac::NSStringToFilePath(cr_versioned_path);
  *framework_shlib_path =
      base::mac::NSStringToFilePath(cr_framework_shlib_path);
  return true;
}

}  // namespace app_mode
