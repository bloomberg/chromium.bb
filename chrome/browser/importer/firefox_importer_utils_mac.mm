// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/firefox_importer_utils.h"

#include "base/file_util.h"
#include "base/path_service.h"

FilePath GetProfilesINI() {
  FilePath app_data_path;
  if (!PathService::Get(base::DIR_APP_DATA, &app_data_path)) {
    return FilePath();
  }
  FilePath ini_file = app_data_path.Append("Firefox").Append("profiles.ini");
  if (!file_util::PathExists(ini_file)) {
    return FilePath();
  }
  return ini_file;
}

FilePath GetFirefoxDylibPath() {
  CFURLRef appURL = nil;
  if (LSFindApplicationForInfo(kLSUnknownCreator,
                              CFSTR("org.mozilla.firefox"),
                              NULL,
                              NULL,
                              &appURL) != noErr) {
    return FilePath();
  }
  NSBundle *ff_bundle = [NSBundle
      bundleWithPath:[reinterpret_cast<const NSURL*>(appURL) path]];
  CFRelease(appURL);
  NSString *ff_library_path = [[ff_bundle executablePath]
                                  stringByDeletingLastPathComponent];

  return FilePath([ff_library_path fileSystemRepresentation]);
}
