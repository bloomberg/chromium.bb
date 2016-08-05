// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main entrypoint for the tool that disables the outdated build detector
// for organic installs of Chrome on Windows XP and Vista. See
// disable_outdated_build_detector.h for more information.

#include <windows.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include "chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector.h"

int WINAPI wWinMain(HINSTANCE /* instance */,
                    HINSTANCE /* unused */,
                    wchar_t* /* command_line */,
                    int /* command_show */) {
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    return static_cast<int>(ExitCode::UNSUPPORTED_OS);

  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(0, nullptr);

  // Don't generate any log files.
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(logging_settings);

  // For reasons, wWinMain returns an int rather than a DWORD. This value
  // eventually makes its way to ExitProcess, which indeed takes an unsigned
  // int.
  return static_cast<int>(
      DisableOutdatedBuildDetector(*base::CommandLine::ForCurrentProcess()));
}
