// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <DbgHelp.h>
#include <string>

#include "chrome_frame/chrome_launcher.h"
#include "chrome_frame/crash_server_init.h"
#include "chrome_frame/update_launcher.h"

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  const wchar_t* cmd_line = ::GetCommandLine();

  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(cmd_line));

  std::wstring update_command(
      update_launcher::GetUpdateCommandFromArguments(cmd_line));

  UINT exit_code = ERROR_FILE_NOT_FOUND;

  if (!update_command.empty())
    exit_code = update_launcher::LaunchUpdateCommand(update_command);
  else if (chrome_launcher::SanitizeAndLaunchChrome(cmd_line))
    exit_code = ERROR_SUCCESS;

  return exit_code;
}
