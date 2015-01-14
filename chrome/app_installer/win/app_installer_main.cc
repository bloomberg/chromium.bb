// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <initguid.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/app_installer/win/app_installer_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/common/user_agent.h"

namespace app_installer {

namespace {

// Log provider UUID. Required for logging to Sawbuck.
// {d82c3b59-bacd-4625-8282-4d570c4dad12}
DEFINE_GUID(kAppInstallerLogProvider,
            0xd82c3b59,
            0xbacd,
            0x4625,
            0x82, 0x82, 0x4d, 0x57, 0x0c, 0x4d, 0xad, 0x12);

const wchar_t kChromeServer[] = L"chrome.google.com";

const wchar_t kInlineInstallDetail[] = L"/webstore/inlineinstall/detail/";

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

  // Get the inline install data for this app. We need to set the user agent
  // string to be Chrome's, otherwise webstore will serve a different response
  // (since inline installs don't work on non-Chrome).
  std::vector<uint8_t> response_data;
  if (!FetchUrl(base::SysUTF8ToWide(content::BuildUserAgentFromProduct(
                    chrome::VersionInfo().ProductNameAndVersionForUserAgent())),
                kChromeServer, 0,
                kInlineInstallDetail + base::SysUTF8ToWide(app_id),
                &response_data) ||
      response_data.empty()) {
    return COULD_NOT_FETCH_INLINE_INSTALL_DATA;
  }

  // Check that the response is valid UTF-8.
  std::string inline_install_json(response_data.begin(), response_data.end());
  if (!base::StreamingUtf8Validator::Validate(inline_install_json))
    return COULD_NOT_PARSE_INLINE_INSTALL_DATA;

  // Parse the data to check it's valid JSON. The download page will just eval
  // it.
  base::JSONReader json_reader;
  scoped_ptr<base::Value> inline_install_data(
      json_reader.ReadToValue(inline_install_json));
  if (!inline_install_data) {
    LOG(ERROR) << json_reader.GetErrorMessage();
    return COULD_NOT_PARSE_INLINE_INSTALL_DATA;
  }

  base::FilePath chrome_path =
      chrome_launcher_support::GetAnyChromePath(is_canary);
  // If none found, show EULA, download, and install Chrome.
  if (chrome_path.empty()) {
    ExitCode get_chrome_result = GetChrome(is_canary, inline_install_json);
    if (get_chrome_result != SUCCESS)
      return get_chrome_result;

    chrome_path = chrome_launcher_support::GetAnyChromePath(is_canary);
    if (chrome_path.empty())
      return COULD_NOT_FIND_CHROME;
  }

  base::CommandLine cmd(chrome_path);
  cmd.AppendSwitchASCII(kInstallChromeApp, app_id);
  DVLOG(1) << "Install command: " << cmd.GetCommandLineString();
  bool launched = base::LaunchProcess(cmd, base::LaunchOptions()).IsValid();
  DVLOG(1) << "Launch " << (launched ? "success." : "failed.");

  return SUCCESS;
}

}  // namespace app_installer
