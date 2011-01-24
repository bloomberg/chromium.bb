// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_UPDATE_LAUNCHER_H_
#define CHROME_FRAME_UPDATE_LAUNCHER_H_
#pragma once

#include <string>

typedef unsigned long DWORD;  // NOLINT

namespace update_launcher {

// If the command line asks us to run a command via the ProcessLauncher, returns
// the command ID. Otherwise, returns an empty string.
std::wstring GetUpdateCommandFromArguments(const wchar_t* command_line);

// Launches the specified command via the ProcessLauncher, waits for the process
// to exit, and returns the exit code for chrome_launcher (normally the launched
// command's exit code, or 0xFF in case of failure to launch).
DWORD LaunchUpdateCommand(const std::wstring& command);

}  // namespace process_launcher

#endif  // CHROME_FRAME_UPDATE_LAUNCHER_H_
