// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"

class CommandLine;

#if defined(OS_WIN)
class ProcessSingleton;
#endif

// This class contains the actions that need to be performed when an upgrade
// is required. This involves mainly swapping the chrome exe and relaunching
// the new browser.
class Upgrade {
 public:
#if defined(OS_WIN)
  // Possible results of ShowTryChromeDialog().
  enum TryResult {
    TRY_CHROME,          // Launch chrome right now.
    NOT_NOW,             // Don't launch chrome. Exit now.
    UNINSTALL_CHROME,    // Initiate chrome uninstall and exit.
    DIALOG_ERROR,        // An error occurred creating the dialog.
    COUNT
  };

  // Check if current chrome.exe is already running as a browser process by
  // trying to create a Global event with name same as full path of chrome.exe.
  // This method caches the handle to this event so on subsequent calls also
  // it can first close the handle and check for any other process holding the
  // handle to the event.
  static bool IsBrowserAlreadyRunning();

  // If the new_chrome.exe exists (placed by the installer then is swapped
  // to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
  // is no new_chrome.exe or the swap fails the return is false;
  static bool SwapNewChromeExeIfPresent();

  // Combines the two methods, RelaunchChromeBrowser and
  // SwapNewChromeExeIfPresent, to perform the rename and relaunch of
  // the browser. Note that relaunch does NOT exit the existing browser process.
  // If this is called before message loop is executed, simply exit the main
  // function. If browser is already running, you will need to exit it.
  static bool DoUpgradeTasks(const CommandLine& command_line);

  // Shows a modal dialog asking the user to give chrome another try. See
  // above for the possible outcomes of the function. This is an experimental,
  // non-localized dialog.
  // |version| can be 0, 1 or 2 and selects what strings to present.
  // |process_singleton| needs to be valid and it will be locked while
  // the dialog is shown.
  static TryResult ShowTryChromeDialog(size_t version,
                                       ProcessSingleton* process_singleton);
#endif  // OS_WIN

  // Launches chrome again simulating a 'user' launch. If chrome could not
  // be launched the return is false.
  static bool RelaunchChromeBrowser(const CommandLine& command_line);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  static void SaveLastModifiedTimeOfExe();
#endif

  static void SetNewCommandLine(CommandLine* new_command_line) {
    // Takes ownership of the pointer.
    new_command_line_ = new_command_line;
  }

  // Launches a new instance of the browser if the current instance in
  // persistent mode an upgrade is detected.
  static void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

  // Windows:
  //  Checks if chrome_new.exe is present in the current instance's install.
  // Linux:
  //  Checks if the last modified time of chrome is newer than that of the
  //  current running instance.
  static bool IsUpdatePendingRestart();

 private:
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  static double GetLastModifiedTimeOfExe();
  static double saved_last_modified_time_of_exe_;
#endif
  static CommandLine* new_command_line_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Upgrade);
};

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_H_
