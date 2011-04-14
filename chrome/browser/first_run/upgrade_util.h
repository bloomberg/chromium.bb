// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
#pragma once

#include "build/build_config.h"

class CommandLine;

#if defined(OS_WIN)
class ProcessSingleton;
#endif

namespace upgrade_util {

void SetNewCommandLine(CommandLine* new_command_line);

// Launches a new instance of the browser if the current instance in persistent
// mode an upgrade is detected.
void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

// Launches chrome again simulating a 'user' launch. If chrome could not be
// launched the return is false.
bool RelaunchChromeBrowser(const CommandLine& command_line);

// Windows:
//  Checks if chrome_new.exe is present in the current instance's install.
// Linux:
//  Checks if the last modified time of chrome is newer than that of the current
//  running instance.
bool IsUpdatePendingRestart();

#if defined(OS_WIN)
// Check if current chrome.exe is already running as a browser process by
// trying to create a Global event with name same as full path of chrome.exe.
// This method caches the handle to this event so on subsequent calls also
// it can first close the handle and check for any other process holding the
// handle to the event.
bool IsBrowserAlreadyRunning();

// If the new_chrome.exe exists (placed by the installer then is swapped
// to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
// is no new_chrome.exe or the swap fails the return is false;
bool SwapNewChromeExeIfPresent();

// Combines the two methods, RelaunchChromeBrowser and
// SwapNewChromeExeIfPresent, to perform the rename and relaunch of
// the browser. Note that relaunch does NOT exit the existing browser process.
// If this is called before message loop is executed, simply exit the main
// function. If browser is already running, you will need to exit it.
bool DoUpgradeTasks(const CommandLine& command_line);

#endif  // OS_WIN

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void SaveLastModifiedTimeOfExe();

double GetLastModifiedTimeOfExe();
#endif

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
