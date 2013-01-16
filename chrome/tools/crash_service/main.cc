// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/crash_service/crash_service.h"

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

const wchar_t kStandardLogFile[] = L"operation_log.txt";

bool GetCrashServiceDirectory(FilePath* dir) {
  FilePath temp_dir;
  if (!file_util::GetTempDir(&temp_dir))
    return false;
  temp_dir = temp_dir.Append(L"chrome_crashes");
  if (!file_util::PathExists(temp_dir)) {
    if (!file_util::CreateDirectory(temp_dir))
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

  CommandLine::Init(0, NULL);

  // We use/create a directory under the user's temp folder, for logging.
  FilePath operating_dir;
  GetCrashServiceDirectory(&operating_dir);
  FilePath log_file = operating_dir.Append(kStandardLogFile);

  // Logging to stderr (to help with debugging failures on the
  // buildbots) and to a file.
  logging::InitLogging(
      log_file.value().c_str(),
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // Logging with pid, tid and timestamp.
  logging::SetLogItems(true, true, true, false);

  VLOG(1) << "session start. cmdline is [" << cmd_line << "]";

  CrashService crash_service(operating_dir.value());
  if (!crash_service.Initialize(::GetCommandLineW()))
    return 1;

  VLOG(1) << "ready to process crash requests";

  // Enter the message loop.
  int retv = crash_service.ProcessingLoop();
  // Time to exit.
  VLOG(1) << "session end. return code is " << retv;
  return retv;
}
