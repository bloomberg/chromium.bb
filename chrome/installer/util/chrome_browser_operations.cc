// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_operations.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserOperations::ReadOptions(
    const MasterPreferences& prefs,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  bool pref_value;

  if (prefs.GetBool(master_preferences::kMultiInstall, &pref_value) &&
      pref_value) {
    options->insert(kOptionMultiInstall);
  }
}

void ChromeBrowserOperations::ReadOptions(
    const CommandLine& uninstall_command,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  if (uninstall_command.HasSwitch(switches::kMultiInstall))
    options->insert(kOptionMultiInstall);
}

void ChromeBrowserOperations::AddKeyFiles(
    const std::set<std::wstring>& options,
    std::vector<FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(FilePath(installer::kChromeDll));
}

void ChromeBrowserOperations::AddComDllList(
    const std::set<std::wstring>& options,
    std::vector<FilePath>* com_dll_list) const {
}

void ChromeBrowserOperations::AppendUninstallFlags(
    const std::set<std::wstring>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  if (options.find(kOptionMultiInstall) != options.end()) {
    // Add --multi-install if it isn't already there.
    if (!cmd_line->HasSwitch(switches::kMultiInstall))
      cmd_line->AppendSwitch(switches::kMultiInstall);

    // --chrome is only needed in multi-install.
    cmd_line->AppendSwitch(switches::kChrome);
  }
}

void ChromeBrowserOperations::AppendRenameFlags(
    const std::set<std::wstring>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  // Add --multi-install if it isn't already there.
  if (options.find(kOptionMultiInstall) != options.end() &&
      !cmd_line->HasSwitch(switches::kMultiInstall)) {
    cmd_line->AppendSwitch(switches::kMultiInstall);
  }
}

bool ChromeBrowserOperations::SetChannelFlags(
    const std::set<std::wstring>& options,
    bool set,
    ChannelInfo* channel_info) const {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  return channel_info->SetChrome(set);
#else
  return false;
#endif
}

bool ChromeBrowserOperations::ShouldCreateUninstallEntry(
    const std::set<std::wstring>& options) const {
  return true;
}

}  // namespace installer
