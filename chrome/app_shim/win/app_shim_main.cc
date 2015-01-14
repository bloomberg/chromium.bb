// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"

namespace {

// Return codes.
const int kOK = 0;
const int kNoProgram = 1;
const int kLaunchFailure = 2;

// Command-line switches.
const char kChromeSxS[] = "chrome-sxs";

}  // namespace

// This program runs chrome.exe, passing its arguments on to the Chrome binary.
// It uses the Windows registry to find chrome.exe (and hence it only works if
// Chrome/Chromium has been properly installed). It terminates as soon as the
// program is launched. It is intended to allow file types to be associated with
// Chrome apps, with a custom name (and in some cases, icon) rather than simply
// the name of the Chrome binary.
//
// Usage: app_shim_win [--chrome-sxs] -- [CHROME_ARGS...]
//
// The -- is required if switches are to be passed to Chrome; any switches
// before the -- will be interpreted by app_shim_win, and not passed to Chrome.
//
// If --chrome-sxs is specified, looks for the SxS (Canary) build of Chrome.
// This will always fail for Chromium.
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* /*command_line*/,
                    int show_command) {
  base::CommandLine::Init(0, nullptr);

  // Log to stderr. Otherwise it will log to a file by default.
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(logging_settings);

  // Get the command-line for the Chrome binary.
  // --chrome-sxs on the command line means we should run the SxS binary.
  bool is_sxs = base::CommandLine::ForCurrentProcess()->HasSwitch(kChromeSxS);
  base::FilePath chrome_path =
      chrome_launcher_support::GetAnyChromePath(is_sxs);
  if (chrome_path.empty()) {
    LOG(ERROR) << "Could not find chrome.exe path in the registry.";
    return kNoProgram;
  }
  base::CommandLine command_line(chrome_path);

  // Get the command-line arguments for the subprocess, consisting of the
  // arguments (but not switches) to this binary. This gets everything after the
  // "--".
  for (const auto& arg : base::CommandLine::ForCurrentProcess()->GetArgs())
    command_line.AppendArgNative(arg);

  if (!base::LaunchProcess(command_line, base::LaunchOptions()).IsValid()) {
    LOG(ERROR) << "Could not run chrome.exe.";
    return kLaunchFailure;
  }

  return kOK;
}
