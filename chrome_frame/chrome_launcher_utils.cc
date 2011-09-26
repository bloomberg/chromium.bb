// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_launcher_utils.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_frame_automation.h"

namespace {

const char kUpdateCommandFlag[] = "--update-cmd";

// Searches for the path to chrome_launcher.exe. Will return false if this
// executable cannot be found, otherwise the command line will be placed in
// |command_line|.
bool CreateChromeLauncherCommandLine(scoped_ptr<CommandLine>* command_line) {
  DCHECK(command_line);
  bool success = false;
  // The launcher EXE will be in the same directory as the Chrome Frame DLL,
  // so create a full path to it based on this assumption.
  FilePath module_path;
  if (PathService::Get(base::FILE_MODULE, &module_path)) {
    FilePath current_dir = module_path.DirName();
    FilePath chrome_launcher = current_dir.Append(
        chrome_launcher::kLauncherExeBaseName);
    if (file_util::PathExists(chrome_launcher)) {
      command_line->reset(new CommandLine(chrome_launcher));
      success = true;
    }
  }

  if (!success) {
    NOTREACHED() << "Could not find " << chrome_launcher::kLauncherExeBaseName
                 << " in output dir.";
  }

  return success;
}

}  // namespace

namespace chrome_launcher {

const wchar_t kLauncherExeBaseName[] = L"chrome_launcher.exe";

bool CreateUpdateCommandLine(const std::wstring& update_command,
                             scoped_ptr<CommandLine>* command_line) {
  DCHECK(command_line);
  bool success = false;

  if (CreateChromeLauncherCommandLine(command_line)) {
    (*command_line)->AppendArg(kUpdateCommandFlag);
    (*command_line)->AppendArg(WideToASCII(update_command));
    success = true;
  }

  return success;
}

bool CreateLaunchCommandLine(scoped_ptr<CommandLine>* command_line) {
  DCHECK(command_line);

  // Shortcut for OS versions that don't need the integrity broker.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    command_line->reset(new CommandLine(GetChromeExecutablePath()));
    return true;
  }

  return CreateChromeLauncherCommandLine(command_line);
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
