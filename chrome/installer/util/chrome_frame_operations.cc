// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_frame_operations.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeFrameOperations::ReadOptions(const MasterPreferences& prefs,
                                        std::set<base::string16>* options)
    const {
  DCHECK(options);

  static const struct PrefToOption {
    const char* pref_name;
    const wchar_t* option_name;
  } map[] = {
    { master_preferences::kMultiInstall, kOptionMultiInstall }
  };

  bool pref_value;

  for (const PrefToOption* scan = &map[0], *end = &map[arraysize(map)];
       scan != end; ++scan) {
    if (prefs.GetBool(scan->pref_name, &pref_value) && pref_value)
      options->insert(scan->option_name);
  }
}

void ChromeFrameOperations::ReadOptions(const CommandLine& uninstall_command,
                                        std::set<base::string16>* options)
    const {
  DCHECK(options);

  static const struct FlagToOption {
    const char* flag_name;
    const wchar_t* option_name;
  } map[] = {
    { switches::kMultiInstall, kOptionMultiInstall }
  };

  for (const FlagToOption* scan = &map[0], *end = &map[arraysize(map)];
       scan != end; ++scan) {
    if (uninstall_command.HasSwitch(scan->flag_name))
      options->insert(scan->option_name);
  }
}

void ChromeFrameOperations::AddKeyFiles(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(installer::kChromeFrameDll));
  key_files->push_back(base::FilePath(installer::kChromeFrameHelperExe));
}

void ChromeFrameOperations::AddComDllList(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* com_dll_list) const {
  DCHECK(com_dll_list);
  com_dll_list->push_back(base::FilePath(installer::kChromeFrameDll));
}

void ChromeFrameOperations::AppendProductFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);
  bool is_multi_install = options.find(kOptionMultiInstall) != options.end();

  // Add --multi-install if it isn't already there.
  if (is_multi_install && !cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);

  // --chrome-frame is always needed.
  cmd_line->AppendSwitch(switches::kChromeFrame);
}

void ChromeFrameOperations::AppendRenameFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);
  bool is_multi_install = options.find(kOptionMultiInstall) != options.end();

  // Add --multi-install if it isn't already there.
  if (is_multi_install && !cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);

  // --chrome-frame is needed for single installs.
  if (!is_multi_install)
    cmd_line->AppendSwitch(switches::kChromeFrame);
}

bool ChromeFrameOperations::SetChannelFlags(
    const std::set<base::string16>& options,
    bool set,
    ChannelInfo* channel_info) const {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  bool modified = channel_info->SetChromeFrame(set);

  // Unconditionally remove the legacy -readymode flag.
  modified |= channel_info->SetReadyMode(false);

  return modified;
#else
  return false;
#endif
}

bool ChromeFrameOperations::ShouldCreateUninstallEntry(
    const std::set<base::string16>& options) const {
  return true;
}

void ChromeFrameOperations::AddDefaultShortcutProperties(
    BrowserDistribution* dist,
    const base::FilePath& target_exe,
    ShellUtil::ShortcutProperties* properties) const {
  NOTREACHED() << "Chrome Frame does not create shortcuts.";
}

void ChromeFrameOperations::LaunchUserExperiment(
    const base::FilePath& setup_path,
    const std::set<base::string16>& options,
    InstallStatus status,
    bool system_level) const {
  // No experiments yet.  If adding some in the future, need to have
  // ChromeFrameDistribution::HasUserExperiments() return true.
  NOTREACHED();
}

}  // namespace installer
