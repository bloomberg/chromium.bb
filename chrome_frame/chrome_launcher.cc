// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_launcher.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_frame_automation.h"

namespace chrome_launcher {

const wchar_t kLauncherExeBaseName[] = L"chrome_launcher.exe";

// These are the switches we will allow (along with their values) in the
// safe-for-Low-Integrity version of the Chrome command line.
const char* kAllowedSwitches[] = {
  switches::kAutomationClientChannelID,
  switches::kChromeFrame,
  switches::kDisableMetrics,
  switches::kEnableRendererAccessibility,
  switches::kEnableExperimentalExtensionApis,
  switches::kNoErrorDialogs,
  switches::kNoFirstRun,
  switches::kUserDataDir,
};

CommandLine* CreateLaunchCommandLine() {
  // TODO(joi) As optimization, could launch Chrome directly when running at
  // medium integrity.  (Requires bringing in code to read SIDs, etc.)

  // The launcher EXE will be in the same directory as the Chrome Frame DLL,
  // so create a full path to it based on this assumption.  Since our unit
  // tests also use this function, and live in the directory above, we test
  // existence of the file and try the path that includes the /servers/
  // directory if needed.
  FilePath module_path;
  if (PathService::Get(base::FILE_MODULE, &module_path)) {
    FilePath current_dir = module_path.DirName();
    FilePath same_dir_path = current_dir.Append(kLauncherExeBaseName);
    if (file_util::PathExists(same_dir_path)) {
      return new CommandLine(same_dir_path);
    } else {
      FilePath servers_path =
          current_dir.Append(L"servers").Append(kLauncherExeBaseName);
      DCHECK(file_util::PathExists(servers_path)) <<
          "What module is this? It's not in 'servers' or main output dir.";
      return new CommandLine(servers_path);
    }
  } else {
    NOTREACHED();
    return NULL;
  }
}

void SanitizeCommandLine(const CommandLine& original, CommandLine* sanitized) {
  int num_sanitized_switches = 0;
  for (int i = 0; i < arraysize(kAllowedSwitches); ++i) {
    const char* current_switch = kAllowedSwitches[i];
    if (original.HasSwitch(current_switch)) {
      ++num_sanitized_switches;
      std::wstring switch_value = original.GetSwitchValue(current_switch);
      if (0 == switch_value.length()) {
        sanitized->AppendSwitch(current_switch);
      } else {
        sanitized->AppendSwitchWithValue(current_switch, switch_value);
      }
    }
  }
  if (num_sanitized_switches != original.GetSwitchCount()) {
    NOTREACHED();
    LOG(ERROR) << "Original command line from Low Integrity had switches "
        << "that are not on our whitelist.";
  }
}

bool SanitizeAndLaunchChrome(const wchar_t* command_line) {
  std::wstring command_line_with_program(L"dummy.exe ");
  command_line_with_program += command_line;
  CommandLine original = CommandLine::FromString(command_line_with_program);
  CommandLine sanitized(GetChromeExecutablePath());
  SanitizeCommandLine(original, &sanitized);

  return base::LaunchApp(sanitized.command_line_string(), false, false, NULL);
}

FilePath GetChromeExecutablePath() {
  FilePath cur_path;
  PathService::Get(base::DIR_MODULE, &cur_path);
  cur_path = cur_path.Append(chrome::kBrowserProcessExecutableName);

  // The installation model for Chrome places the DLLs in a versioned
  // sub-folder one down from the Chrome executable. If we fail to find
  // chrome.exe in the current path, try looking one up and launching that
  // instead.
  if (!file_util::PathExists(cur_path)) {
    PathService::Get(base::DIR_MODULE, &cur_path);
    cur_path = cur_path.DirName().Append(chrome::kBrowserProcessExecutableName);
  }

  return cur_path;
}

}  // namespace chrome_launcher

// Entrypoint that implements the logic of chrome_launcher.exe.
int CALLBACK CfLaunchChrome() {
  if (chrome_launcher::SanitizeAndLaunchChrome(::GetCommandLine())) {
    return ERROR_SUCCESS;
  } else {
    return ERROR_OPEN_FAILED;
  }
}

// Compile-time check to see that the type CfLaunchChromeProc is correct.
#ifndef NODEBUG
namespace {
chrome_launcher::CfLaunchChromeProc cf_launch_chrome = CfLaunchChrome;
}  // namespace
#endif  // NODEBUG
