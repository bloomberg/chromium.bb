// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_
#define CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_

#include <string>

class CommandLine;
class FilePath;

namespace chrome_launcher {

// The base name of the chrome_launcher.exe file.
extern const wchar_t kLauncherExeBaseName[];

// Creates a command line suitable for launching Chrome.  You can add any
// flags needed before launching.
//
// The command-line may use the Chrome executable directly, or use an in-between
// process if needed for security/elevation purposes.  You must delete the
// returned command line.
CommandLine* CreateLaunchCommandLine();

// Creates a command line suitable for launching the specified command through
// Google Update.
//
// You must delete the returned command line.
CommandLine* CreateUpdateCommandLine(const std::wstring& update_command);

// Returns the full path to the Chrome executable.
FilePath GetChromeExecutablePath();

}  // namespace chrome_launcher

#endif  // CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_
