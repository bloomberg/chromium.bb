// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#include <string>  // TODO(viettrungluu): only needed for temporary hack

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/app_mode_common.h"

extern "C" {

// |ChromeAppModeStart()| is the point of entry into the framework from the app
// mode loader.
__attribute__((visibility("default")))
int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info);

// TODO(viettrungluu): put this in a header file somewhere.
int ChromeMain(int argc, char** argv);

}  // extern "C"

int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info) {
  if (info->major_version < app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "App Mode Loader too old.");
    return 1;
  }
  if (info->major_version > app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "Browser Framework too old to load App Shortcut.");
    return 1;
  }

  RAW_CHECK(info->chrome_versioned_path);
  FilePath* chrome_versioned_path = new FilePath(info->chrome_versioned_path);
  RAW_CHECK(!chrome_versioned_path->empty());
  chrome::SetOverrideVersionedDirectory(chrome_versioned_path);

  // TODO(viettrungluu): do something intelligent with data
//  return ChromeMain(info->argc, info->argv);
  // For now, a cheesy hack instead.
  RAW_CHECK(info->app_mode_url);
  std::string argv1(std::string("--app=") + info->app_mode_url);
  RAW_CHECK(info->app_mode_id);
  std::string argv2(std::string("--user-data-dir=/tmp/") + info->app_mode_id);
  char* argv[] = { info->argv[0],
                   const_cast<char*>(argv1.c_str()),
                   const_cast<char*>(argv2.c_str()) };
  return ChromeMain(static_cast<int>(arraysize(argv)), argv);
}
