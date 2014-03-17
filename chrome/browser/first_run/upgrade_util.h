// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <string>
#endif

#if !defined(OS_CHROMEOS)

namespace base {
class CommandLine;
}

namespace upgrade_util {

// Launches Chrome again simulating a "user" launch. If Chrome could not be
// launched, returns false.
bool RelaunchChromeBrowser(const base::CommandLine& command_line);

#if defined(OS_WIN)

extern const char kRelaunchModeMetro[];
extern const char kRelaunchModeDesktop[];
extern const char kRelaunchModeDefault[];

enum RelaunchMode {
  RELAUNCH_MODE_METRO = 0,
  RELAUNCH_MODE_DESKTOP = 1,
  // Default mode indicates caller is not sure which mode to launch.
  RELAUNCH_MODE_DEFAULT = 2,
};

std::string RelaunchModeEnumToString(const RelaunchMode& relaunch_mode);

RelaunchMode RelaunchModeStringToEnum(const std::string& relaunch_mode);

// Like RelaunchChromeBrowser() but for Windows 8 it will read pref and restart
// chrome accordingly in desktop or metro mode.
bool RelaunchChromeWithMode(const base::CommandLine& command_line,
                            const RelaunchMode& relaunch_mode);

#endif

#if !defined(OS_MACOSX)

void SetNewCommandLine(base::CommandLine* new_command_line);

// Launches a new instance of the browser if the current instance in persistent
// mode an upgrade is detected.
void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

// Windows:
//  Checks if chrome_new.exe is present in the current instance's install.
// Linux:
//  Checks if the last modified time of chrome is newer than that of the current
//  running instance.
bool IsUpdatePendingRestart();

#endif  // !defined(OS_MACOSX)

}  // namespace upgrade_util

#endif  // !defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
