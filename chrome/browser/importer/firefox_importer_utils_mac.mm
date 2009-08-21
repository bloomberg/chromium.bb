// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/firefox_importer_utils.h"

#include "base/file_util.h"

FilePath GetProfilesINI() {
  FilePath ini_file;
  NSArray* dirs =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                          NSUserDomainMask, YES);
  if ([dirs count]) {
    NSString* app_support_dir = [dirs objectAtIndex:0];
    NSString* firefox_dir = [app_support_dir
        stringByAppendingPathComponent:@"Firefox"];
    NSString* profiles_ini = [firefox_dir
        stringByAppendingPathComponent:@"profiles.ini"];
    if (profiles_ini) {
      ini_file = FilePath([profiles_ini fileSystemRepresentation]);
    }
  }
  
  if (file_util::PathExists(ini_file))
    return ini_file;

  return FilePath();
}
