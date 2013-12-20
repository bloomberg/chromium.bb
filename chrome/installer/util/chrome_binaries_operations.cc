// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_binaries_operations.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBinariesOperations::ReadOptions(const MasterPreferences& prefs,
                                           std::set<base::string16>* options)
    const {
  DCHECK(options);
  options->insert(kOptionMultiInstall);
}

void ChromeBinariesOperations::ReadOptions(const CommandLine& uninstall_command,
                                           std::set<base::string16>* options)
    const {
  DCHECK(options);
  options->insert(kOptionMultiInstall);
}

void ChromeBinariesOperations::AddKeyFiles(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(installer::kChromeDll));
}

void ChromeBinariesOperations::AddComDllList(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* com_dll_list) const {
}

void ChromeBinariesOperations::AppendProductFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  DCHECK(options.find(kOptionMultiInstall) != options.end());

  // Add --multi-install if it isn't already there.
  if (!cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);
}

void ChromeBinariesOperations::AppendRenameFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  DCHECK(options.find(kOptionMultiInstall) != options.end());

  // Add --multi-install if it isn't already there.
  if (!cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);
}

bool ChromeBinariesOperations::SetChannelFlags(
    const std::set<base::string16>& options,
    bool set,
    ChannelInfo* channel_info) const {
  return false;
}

bool ChromeBinariesOperations::ShouldCreateUninstallEntry(
    const std::set<base::string16>& options) const {
  return false;
}

void ChromeBinariesOperations::AddDefaultShortcutProperties(
    BrowserDistribution* dist,
    const base::FilePath& target_exe,
    ShellUtil::ShortcutProperties* properties) const {
  NOTREACHED() << "Chrome Binaries do not create shortcuts.";
}

void ChromeBinariesOperations::LaunchUserExperiment(
    const base::FilePath& setup_path,
    const std::set<base::string16>& options,
    InstallStatus status,
    bool system_level) const {
  // Not meaningful to have binaries run experiments.
  NOTREACHED();
}

}  // namespace installer
