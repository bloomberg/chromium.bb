// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_
#define CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_

#include <string>
#include "base/memory/scoped_ptr.h"

class CommandLine;

namespace base {
class FilePath;
}

namespace chrome_launcher {

// The base name of the chrome_launcher.exe file.
extern const wchar_t kLauncherExeBaseName[];

// Creates a command line suitable for launching Chrome.  You can add any
// flags needed before launching.
//
// The command-line may use the Chrome executable directly, or use an in-between
// process if needed for security/elevation purposes.
//
// On success, returns true and populates command_line, which must be non-NULL,
// with the launch command line.
bool CreateLaunchCommandLine(scoped_ptr<CommandLine>* command_line);

// Creates a command line suitable for launching the specified command through
// Google Update.
//
// On success, returns true and populates command_line, which must be non-NULL,
// with the update command line.
bool CreateUpdateCommandLine(const std::wstring& update_command,
                             scoped_ptr<CommandLine>* command_line);

// Returns the full path to the Chrome executable.
base::FilePath GetChromeExecutablePath();

}  // namespace chrome_launcher

#endif  // CHROME_FRAME_CHROME_LAUNCHER_UTILS_H_
