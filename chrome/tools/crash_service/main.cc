// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/tools/crash_service.h"

namespace {

const wchar_t kStandardLogFile[] = L"operation_log.txt";

bool GetCrashServiceDirectory(base::FilePath* dir) {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir))
    return false;
  temp_dir = temp_dir.Append(L"chrome_crashes");
  if (!base::PathExists(temp_dir)) {
    if (!base::CreateDirectory(temp_dir))
      return false;
  }
  *dir = temp_dir;
  return true;
}

}  // namespace.

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmd_line,
                       int show_mode) {
  // Manages the destruction of singletons.
  base::AtExitManager exit_manager;

  base::CommandLine::Init(0, NULL);

  chrome::RegisterPathProvider();

  // We use/create a directory under the user's temp folder, for logging.
  base::FilePath operating_dir;
  GetCrashServiceDirectory(&operating_dir);
  base::FilePath log_file = operating_dir.Append(kStandardLogFile);

  // Logging to stderr (to help with debugging failures on the
  // buildbots) and to a file.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_file.value().c_str();
  logging::InitLogging(settings);
  // Logging with pid, tid and timestamp.
  logging::SetLogItems(true, true, true, false);

  VLOG(1) << "session start. cmdline is [" << cmd_line << "]";

  base::FilePath dumps_path;
  if (!PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path)) {
    LOG(ERROR) << "could not get DIR_CRASH_DUMPS";
    return 1;
  }

  breakpad::CrashService crash_service;
  if (!crash_service.Initialize(operating_dir, dumps_path))
    return 1;

  VLOG(1) << "ready to process crash requests";

  // Enter the message loop.
  int retv = crash_service.ProcessingLoop();
  // Time to exit.
  VLOG(1) << "session end. return code is " << retv;
  return retv;
}
