// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_frame_operations.h"

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
    { master_preferences::kCeee, kOptionCeee },
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
    { switches::kCeee, kOptionCeee },
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
    std::vector<FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(FilePath(installer::kChromeFrameDll));
  if (options.find(kOptionCeee) != options.end()) {
    key_files->push_back(FilePath(installer::kCeeeIeDll));
    key_files->push_back(FilePath(installer::kCeeeBrokerExe));
  }
}

void ChromeFrameOperations::AddComDllList(
    const std::set<std::wstring>& options,
    std::vector<FilePath>* com_dll_list) const {
  DCHECK(com_dll_list);
  std::vector<FilePath> dll_list;
  com_dll_list->push_back(FilePath(installer::kChromeFrameDll));
  if (options.find(kOptionCeee) != options.end()) {
    com_dll_list->push_back(FilePath(installer::kCeeeInstallHelperDll));
    com_dll_list->push_back(FilePath(installer::kCeeeIeDll));
  }
}

void ChromeFrameOperations::AppendProductFlags(
    const std::set<std::wstring>& options,
    CommandLine* uninstall_command) const {
  DCHECK(uninstall_command);
  uninstall_command->AppendSwitch(switches::kChromeFrame);

  if (options.find(kOptionCeee) != options.end())
    uninstall_command->AppendSwitch(switches::kCeee);

  if (options.find(kOptionMultiInstall) != options.end()) {
    if (!uninstall_command->HasSwitch(switches::kMultiInstall))
      uninstall_command->AppendSwitch(switches::kMultiInstall);

    // ready-mode is only supported in multi-installs of Chrome Frame.
    if (options.find(kOptionReadyMode) != options.end())
      uninstall_command->AppendSwitch(switches::kChromeFrameReadyMode);
  }
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
  modified |= channel_info->SetCeee(
      set && options.find(kOptionCeee) != options.end());

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

}  // namespace installer
