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

// Remove ready-mode if not multi-install.
void ChromeFrameOperations::NormalizeOptions(
    std::set<std::wstring>* options) const {
  std::set<std::wstring>::iterator ready_mode(options->find(kOptionReadyMode));
  if (ready_mode != options->end() &&
      options->find(kOptionMultiInstall) == options->end()) {
    LOG(WARNING) << "--ready-mode option does not apply when --multi-install "
                    "is not also specified; ignoring.";
    options->erase(ready_mode);
  }
}

void ChromeFrameOperations::ReadOptions(
    const MasterPreferences& prefs,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  static const struct PrefToOption {
    const char* pref_name;
    const wchar_t* option_name;
  } map[] = {
    { master_preferences::kChromeFrameReadyMode, kOptionReadyMode },
    { master_preferences::kMultiInstall, kOptionMultiInstall }
  };

  bool pref_value;

  for (const PrefToOption* scan = &map[0], *end = &map[arraysize(map)];
       scan != end; ++scan) {
    if (prefs.GetBool(scan->pref_name, &pref_value) && pref_value)
      options->insert(scan->option_name);
  }

  NormalizeOptions(options);
}

void ChromeFrameOperations::ReadOptions(
    const CommandLine& uninstall_command,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  static const struct FlagToOption {
    const char* flag_name;
    const wchar_t* option_name;
  } map[] = {
    { switches::kChromeFrameReadyMode, kOptionReadyMode },
    { switches::kMultiInstall, kOptionMultiInstall }
  };

  for (const FlagToOption* scan = &map[0], *end = &map[arraysize(map)];
       scan != end; ++scan) {
    if (uninstall_command.HasSwitch(scan->flag_name))
      options->insert(scan->option_name);
  }

  NormalizeOptions(options);
}

void ChromeFrameOperations::AddKeyFiles(
    const std::set<std::wstring>& options,
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(installer::kChromeFrameDll));
  key_files->push_back(base::FilePath(installer::kChromeFrameHelperExe));
}

void ChromeFrameOperations::AddComDllList(
    const std::set<std::wstring>& options,
    std::vector<base::FilePath>* com_dll_list) const {
  DCHECK(com_dll_list);
  com_dll_list->push_back(base::FilePath(installer::kChromeFrameDll));
}

void ChromeFrameOperations::AppendProductFlags(
    const std::set<std::wstring>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);
  bool is_multi_install = options.find(kOptionMultiInstall) != options.end();

  // Add --multi-install if it isn't already there.
  if (is_multi_install && !cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);

  // --chrome-frame is always needed.
  cmd_line->AppendSwitch(switches::kChromeFrame);

  // ready-mode is only supported in multi-installs of Chrome Frame.
  if (is_multi_install && options.find(kOptionReadyMode) != options.end())
    cmd_line->AppendSwitch(switches::kChromeFrameReadyMode);
}

void ChromeFrameOperations::AppendRenameFlags(
    const std::set<std::wstring>& options,
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
    const std::set<std::wstring>& options,
    bool set,
    ChannelInfo* channel_info) const {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  bool modified = channel_info->SetChromeFrame(set);

  // Always remove the options if we're called to remove flags or if the
  // corresponding option isn't set.
  modified |= channel_info->SetReadyMode(
      set && options.find(kOptionReadyMode) != options.end());

  return modified;
#else
  return false;
#endif
}

bool ChromeFrameOperations::ShouldCreateUninstallEntry(
    const std::set<std::wstring>& options) const {
  return options.find(kOptionReadyMode) == options.end();
}

void ChromeFrameOperations::AddDefaultShortcutProperties(
    BrowserDistribution* dist,
    const base::FilePath& target_exe,
    ShellUtil::ShortcutProperties* properties) const {
  NOTREACHED() << "Chrome Frame does not create shortcuts.";
}

}  // namespace installer
