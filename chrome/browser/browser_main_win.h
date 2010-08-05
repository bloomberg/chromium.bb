// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are win32-specific.

#ifndef CHROME_BROWSER_BROWSER_MAIN_WIN_H_
#define CHROME_BROWSER_BROWSER_MAIN_WIN_H_
#pragma once

class CommandLine;

// Handle uninstallation when given the appropriate the command-line switch.
// If |chrome_still_running| is true a modal dialog will be shown asking the
// user to close the other chrome instance.
int DoUninstallTasks(bool chrome_still_running);

// Prepares the localized strings that are going to be displayed to
// the user if the browser process dies. These strings are stored in the
// environment block so they are accessible in the early stages of the
// chrome executable's lifetime.
void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line);

// This method handles the --hide-icons and --show-icons command line options
// for chrome that get triggered by Windows from registry entries
// HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
// functionality so we just ask the users if they want to uninstall Chrome.
int HandleIconsCommands(const CommandLine &parsed_command_line);

// Check if there is any machine level Chrome installed on the current
// machine. If yes and the current Chrome process is user level, we do not
// allow the user level Chrome to run. So we notify the user and uninstall
// user level Chrome.
bool CheckMachineLevelInstall();

#endif  // CHROME_BROWSER_BROWSER_MAIN_WIN_H_
