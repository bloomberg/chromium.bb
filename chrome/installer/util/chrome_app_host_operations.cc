// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_app_host_operations.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeAppHostOperations::ReadOptions(
    const MasterPreferences& prefs,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  bool pref_value;
  if (prefs.GetBool(master_preferences::kMultiInstall, &pref_value) &&
      pref_value) {
    options->insert(kOptionMultiInstall);
  }
  if (prefs.GetBool(master_preferences::kChromeAppLauncher, &pref_value) &&
      pref_value) {
    options->insert(kOptionAppHostIsLauncher);
  }
}

void ChromeAppHostOperations::ReadOptions(
    const CommandLine& uninstall_command,
    std::set<std::wstring>* options) const {
  DCHECK(options);

  if (uninstall_command.HasSwitch(switches::kMultiInstall))
    options->insert(kOptionMultiInstall);
  if (uninstall_command.HasSwitch(switches::kChromeAppLauncher))
    options->insert(kOptionAppHostIsLauncher);
}

void ChromeAppHostOperations::AddKeyFiles(
    const std::set<std::wstring>& options,
    std::vector<base::FilePath>* key_files) const {
}

void ChromeAppHostOperations::AddComDllList(
    const std::set<std::wstring>& options,
    std::vector<base::FilePath>* com_dll_list) const {
}

void ChromeAppHostOperations::AppendProductFlags(
    const std::set<std::wstring>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);
  bool is_multi_install = options.find(kOptionMultiInstall) != options.end();

  // Non-multi-install not supported for the app host.
  DCHECK(is_multi_install);

  // Add --multi-install if it isn't already there.
  if (is_multi_install && !cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);

  // Either --app-launcher or --app-host is always needed.
  if (options.find(kOptionAppHostIsLauncher) != options.end())
    cmd_line->AppendSwitch(switches::kChromeAppLauncher);
  else
    cmd_line->AppendSwitch(switches::kChromeAppHost);
}

void ChromeAppHostOperations::AppendRenameFlags(
    const std::set<std::wstring>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);
  bool is_multi_install = options.find(kOptionMultiInstall) != options.end();

  // Non-multi-install not supported for the app host.
  DCHECK(is_multi_install);

  // Add --multi-install if it isn't already there.
  if (is_multi_install && !cmd_line->HasSwitch(switches::kMultiInstall))
    cmd_line->AppendSwitch(switches::kMultiInstall);
}

bool ChromeAppHostOperations::SetChannelFlags(
    const std::set<std::wstring>& options,
    bool set,
    ChannelInfo* channel_info) const {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  bool modified_app_host = false;
  bool modified_app_launcher = false;
  bool is_app_launcher =
      (options.find(kOptionAppHostIsLauncher) != options.end());
  // If set, then App Host and App Launcher are mutually exclusive.
  // If !set, then remove both.
  modified_app_host = channel_info->SetAppHost(set && !is_app_launcher);
  modified_app_launcher = channel_info->SetAppLauncher(set && is_app_launcher);
  return modified_app_host || modified_app_launcher;
#else
  return false;
#endif
}

bool ChromeAppHostOperations::ShouldCreateUninstallEntry(
    const std::set<std::wstring>& options) const {
  return (options.find(kOptionAppHostIsLauncher) != options.end());
}

void ChromeAppHostOperations::AddDefaultShortcutProperties(
    BrowserDistribution* dist,
    const base::FilePath& target_exe,
    ShellUtil::ShortcutProperties* properties) const {
  if (!properties->has_target())
    properties->set_target(target_exe);

  if (!properties->has_arguments()) {
    CommandLine app_host_args(CommandLine::NO_PROGRAM);
    app_host_args.AppendSwitch(::switches::kShowAppList);
    properties->set_arguments(app_host_args.GetCommandLineString());
  }

  if (!properties->has_icon())
    properties->set_icon(target_exe, dist->GetIconIndex());

  if (!properties->has_app_id()) {
    std::vector<string16> components;
    components.push_back(dist->GetBaseAppId());
    properties->set_app_id(ShellUtil::BuildAppModelId(components));
  }
}

}  // namespace installer
