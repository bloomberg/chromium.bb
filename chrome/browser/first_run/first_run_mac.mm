// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/mac/master_prefs.h"

bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
  // http://crbug.com/48880
  return false;
}

// static
bool FirstRun::IsOrganicFirstRun() {
  std::string brand = keystone_glue::BrandCode();
  return brand.empty() ||
         StartsWithASCII(brand, "GG", true) ||
         StartsWithASCII(brand, "EU", true);
}

// static
void FirstRun::PlatformSetup() {
  // Things that Windows does here (creating a desktop icon, for example) are
  // not needed.
}

// static
FilePath FirstRun::MasterPrefsPath() {
  return master_prefs::MasterPrefsPath();
}
