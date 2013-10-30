// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/breakpad/tools/crash_service.h"

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmd_line,
                       int show_mode) {
  // Manages the destruction of singletons.
  base::AtExitManager exit_manager;

  CommandLine::Init(0, NULL);

  // Logging to stderr.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // Logging with pid, tid and timestamp.
  logging::SetLogItems(true, true, true, false);

  VLOG(1) << "session start. cmdline is [" << cmd_line << "]";

  breakpad::CrashService crash_service;
  if (!crash_service.Initialize(base::FilePath(), base::FilePath()))
    return 1;

  VLOG(1) << "ready to process crash requests";

  // Enter the message loop.
  int retv = crash_service.ProcessingLoop();
  // Time to exit.
  VLOG(1) << "session end. return code is " << retv;
  return retv;
}
