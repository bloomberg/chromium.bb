// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_operations.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/user_experiment.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserOperations::AddKeyFiles(
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(kChromeDll));
}

// Modifies a ShortcutProperties object by adding default values to
// uninitialized members. Tries to assign:
// - target: |chrome_exe|.
// - icon: from |chrome_exe|.
// - icon_index: |dist|'s icon index
// - app_id: the browser model id for the current install.
// - description: |dist|'s description.
void ChromeBrowserOperations::AddDefaultShortcutProperties(
      BrowserDistribution* dist,
      const base::FilePath& target_exe,
      ShellUtil::ShortcutProperties* properties) const {
  if (!properties->has_target())
    properties->set_target(target_exe);

  if (!properties->has_icon())
    properties->set_icon(target_exe, dist->GetIconIndex());

  if (!properties->has_app_id()) {
    properties->set_app_id(
        ShellUtil::GetBrowserModelId(dist, InstallUtil::IsPerUserInstall()));
  }

  if (!properties->has_description())
    properties->set_description(dist->GetAppDescription());
}

void ChromeBrowserOperations::LaunchUserExperiment(
    const base::FilePath& setup_path,
    InstallStatus status,
    bool system_level) const {
  base::CommandLine base_command(setup_path);
  InstallUtil::AppendModeSwitch(&base_command);
  LaunchBrowserUserExperiment(base_command, status, system_level);
}

}  // namespace installer
