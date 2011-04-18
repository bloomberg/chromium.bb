// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/file_path.h"

bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
  // http://crbug.com/48880
  return false;
}

// static
bool FirstRun::IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

// static
void FirstRun::PlatformSetup() {
  // Things that Windows does here (creating a desktop icon, for example) are
  // not needed.
}
