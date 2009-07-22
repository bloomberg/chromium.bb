// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include "base/file_util.h"

FilePath GetProfilesINI() {
  FilePath ini_file;
  // ~/Library/Application Support/Firefox on OS X.
  // This should be changed to NSSearchPathForDirectoriesInDomains().
  // See bug http://code.google.com/p/chromium/issues/detail?id=15455
  const char *home = getenv("HOME");
  if (home && home[0]) {
    ini_file = FilePath(home).Append(
        "Library/Application Support/Firefox/profiles.ini");
  }
  if (file_util::PathExists(ini_file))
    return ini_file;

  return FilePath();
}
