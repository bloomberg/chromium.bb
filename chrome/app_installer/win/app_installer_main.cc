// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <initguid.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "chrome/app_installer/win/app_installer_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"

namespace app_installer {

namespace {

// Log provider UUID. Required for logging to Sawbuck.
// {d82c3b59-bacd-4625-8282-4d570c4dad12}
DEFINE_GUID(kAppInstallerLogProvider,
            0xd82c3b59,
            0xbacd,
            0x4625,
            0x82, 0x82, 0x4d, 0x57, 0x0c, 0x4d, 0xad, 0x12);

}  // namespace

extern "C"
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* command_line,
                    int show_command) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);
  logging::LogEventProvider::Initialize(kAppInstallerLogProvider);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAppId);
  const char* sxs = installer::switches::kChromeSxS;
  // --chrome-sxs on the command line takes precedence over chrome-sxs in the
  // tag.
  bool is_canary = base::CommandLine::ForCurrentProcess()->HasSwitch(sxs);

  // --app-id on the command line inhibits tag parsing altogether.
  if (app_id.empty()) {
    base::FilePath current_exe;
    if (!PathService::Get(base::FILE_EXE, &current_exe))
      return COULD_NOT_GET_FILE_PATH;

    // Get the tag added by dl.google.com. Note that this is passed in via URL
    // parameters when requesting a file to download, so it must be validated
    // before use.
    std::string tag = GetTag(current_exe);
    if (tag.empty())
      return COULD_NOT_READ_TAG;

    DVLOG(1) << "Tag: " << tag;

    std::map<std::string, std::string> parsed_pairs;
    if (!ParseTag(tag, &parsed_pairs))
      return COULD_NOT_PARSE_TAG;

    auto result = parsed_pairs.find(switches::kAppId);
    if (result != parsed_pairs.end())
      app_id = result->second;

    if (!is_canary) {
      result = parsed_pairs.find(sxs);
      is_canary = result != parsed_pairs.end() && result->second == "1";
    }
  }

  if (!IsValidAppId(app_id))
    return INVALID_APP_ID;

  base::FilePath chrome_path = GetChromeExePath(is_canary);
  // If none found, show EULA, download, and install Chrome.
  if (chrome_path.empty()) {
    ExitCode get_chrome_result = GetChrome(is_canary);
    if (get_chrome_result != SUCCESS)
      return get_chrome_result;

    chrome_path = GetChromeExePath(is_canary);
    if (chrome_path.empty())
      return COULD_NOT_FIND_CHROME;
  }

  base::CommandLine cmd(chrome_path);
  cmd.AppendSwitchASCII(kInstallChromeApp, app_id);
  DVLOG(1) << "Install command: " << cmd.GetCommandLineString();
  bool launched = base::LaunchProcess(cmd, base::LaunchOptions(), NULL);
  DVLOG(1) << "Launch " << (launched ? "success." : "failed.");

  return SUCCESS;
}

}  // namespace app_installer
