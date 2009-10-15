// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_LAUNCHER_H_
#define CHROME_FRAME_CHROME_LAUNCHER_H_

#include <string>

class CommandLine;

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

// Fills in a new command line from the flags on this process's command line
// that we are allowing Low Integrity to invoke.
//
// Logs a warning for any flags that were passed that are not allowed to be
// invoked by Low Integrity.
void SanitizeCommandLine(const CommandLine& original, CommandLine* sanitized);

// Given a command-line without an initial program part, launch our associated
// chrome.exe with a sanitized version of that command line. Returns true iff 
// successful.
bool SanitizeAndLaunchChrome(const wchar_t* command_line);

// Returns the full path to the Chrome executable.
std::wstring GetChromeExecutablePath();

// The type of the CfLaunchChrome entrypoint exported from this DLL.
typedef int (__stdcall *CfLaunchChromeProc)();

}  // namespace chrome_launcher

#endif  // CHROME_FRAME_CHROME_LAUNCHER_H_
