// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
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

  RAW_CHECK(!info->chrome_versioned_path.empty());
  FilePath* chrome_versioned_path = new FilePath(info->chrome_versioned_path);
  RAW_CHECK(!chrome_versioned_path->empty());
  chrome::SetOverrideVersionedDirectory(chrome_versioned_path);
  base::mac::SetOverrideOuterBundlePath(info->chrome_outer_bundle_path);
  base::mac::SetOverrideFrameworkBundlePath(
      chrome_versioned_path->Append(chrome::kFrameworkName));

  // This struct is used to communicate information to the Chrome code, prefer
  // this to modifying the command line below.
  struct ShellIntegration::AppModeInfo app_mode_info;
  ShellIntegration::SetAppModeInfo(&app_mode_info);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(info->argv[0]);

  RAW_CHECK(info->app_mode_id.size());
  command_line.AppendSwitchASCII(switches::kAppId, info->app_mode_id);
  command_line.AppendSwitchPath(switches::kUserDataDir, info->user_data_dir);
  // TODO(sail): Use a different flag that doesn't imply Location::LOAD for the
  // extension.
  command_line.AppendSwitchPath(switches::kLoadExtension, info->extension_path);

  int argc = command_line.argv().size();
  char* argv[argc];
  for (int i = 0; i < argc; ++i)
    argv[i] = const_cast<char*>(command_line.argv()[i].c_str());

  return ChromeMain(argc, argv);
}
