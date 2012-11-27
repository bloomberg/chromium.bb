// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are win32-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main.h"

class CommandLine;

namespace chrome {
class RemovableDeviceNotificationsWindowWin;
}  // namespace chrome


// Handle uninstallation when given the appropriate the command-line switch.
// If |chrome_still_running| is true a modal dialog will be shown asking the
// user to close the other chrome instance.
int DoUninstallTasks(bool chrome_still_running);

class ChromeBrowserMainPartsWin : public ChromeBrowserMainParts {
 public:
  explicit ChromeBrowserMainPartsWin(
      const content::MainFunctionParams& parameters);

  virtual ~ChromeBrowserMainPartsWin();

  // BrowserParts overrides.
  virtual void ToolkitInitialized() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;

  // ChromeBrowserMainParts overrides.
  virtual void ShowMissingLocaleMessageBox() OVERRIDE;

  // Prepares the localized strings that are going to be displayed to
  // the user if the browser process dies. These strings are stored in the
  // environment block so they are accessible in the early stages of the
  // chrome executable's lifetime.
  static void PrepareRestartOnCrashEnviroment(
      const CommandLine& parsed_command_line);

  // Registers Chrome with the Windows Restart Manager, which will restore the
  // Chrome session when the computer is restarted after a system update.
  static void RegisterApplicationRestart(
      const CommandLine& parsed_command_line);

  // This method handles the --hide-icons and --show-icons command line options
  // for chrome that get triggered by Windows from registry entries
  // HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
  // functionality so we just ask the users if they want to uninstall Chrome.
  static int HandleIconsCommands(const CommandLine& parsed_command_line);

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  static bool CheckMachineLevelInstall();

  // Sets the TranslationDelegate which provides localized strings to
  // installer_util.
  static void SetupInstallerUtilStrings();

 private:
  scoped_ptr<chrome::RemovableDeviceNotificationsWindowWin>
      removable_device_notifications_window_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsWin);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
