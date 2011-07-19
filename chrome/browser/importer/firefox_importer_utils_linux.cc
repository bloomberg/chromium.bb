// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include "base/file_util.h"

FilePath GetProfilesINI() {
  FilePath ini_file;
  // The default location of the profile folder containing user data is
  // under user HOME directory in .mozilla/firefox folder on Linux.
  FilePath home = file_util::GetHomeDir();
  if (!home.empty()) {
    ini_file = home.Append(".mozilla/firefox/profiles.ini");
  }
  if (file_util::PathExists(ini_file))
    return ini_file;

  return FilePath();
}
