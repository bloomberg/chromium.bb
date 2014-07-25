// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_
#define CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_

namespace base {
class FilePath;
}

namespace chrome_launcher_support {

enum InstallationLevel {
  USER_LEVEL_INSTALLATION,
  SYSTEM_LEVEL_INSTALLATION,
};

enum InstallationState {
  NOT_INSTALLED,
  INSTALLED_AT_USER_LEVEL,
  INSTALLED_AT_SYSTEM_LEVEL,
};

// The app GUID for Chrome App Launcher.
extern const wchar_t kAppLauncherGuid[];

// Returns the path to an existing setup.exe at the specified level, if it can
// be found via Omaha client state.
base::FilePath GetSetupExeForInstallationLevel(InstallationLevel level);

// Returns the path to an installed chrome.exe at the specified level, if it can
// be found via Omaha client state. Prefers the installer from a multi-install,
// but may also return that of a single-install of Chrome if no multi-install
// exists.
base::FilePath GetChromePathForInstallationLevel(InstallationLevel level);

// Returns the path to an installed app_host.exe at the specified level, if
// it can be found via Omaha client state.
base::FilePath GetAppHostPathForInstallationLevel(InstallationLevel level);

// Returns the path to an installed SxS chrome.exe at the specified level, if
// it can be found via Omaha client state.
base::FilePath GetChromeSxSPathForInstallationLevel(InstallationLevel level);

// Returns the path to an installed chrome.exe, or an empty path. Prefers a
// system-level installation to a user-level installation. Uses Omaha client
// state to identify a Chrome installation location.
// The file path returned (if any) is guaranteed to exist.
base::FilePath GetAnyChromePath();

// Returns the path to an installed app_host.exe, or an empty path. Prefers a
// system-level installation to a user-level installation. Uses Omaha client
// state to identify a App Host installation location.
// The file path returned (if any) is guaranteed to exist.
base::FilePath GetAnyAppHostPath();

// Returns the path to an installed SxS chrome.exe, or an empty path. Prefers a
// user-level installation to a system-level installation. Uses Omaha client
// state to identify a Chrome Canary installation location.
// The file path returned (if any) is guaranteed to exist.
base::FilePath GetAnyChromeSxSPath();

// Uninstalls the legacy app launcher by launching setup.exe with the uninstall
// arguments from the App Launcher ClientState registry key. The uninstall will
// run asynchronously.
void UninstallLegacyAppLauncher(InstallationLevel level);

// Returns true if App Host is installed (system-level or user-level),
// or in the same directory as the current executable.
bool IsAppHostPresent();

// Returns the app launcher installation state. If the launcher is installed
// at both system level and user level, system level is returned.
InstallationState GetAppLauncherInstallationState();

// Returns true if App Launcher is installed (system-level or user-level).
bool IsAppLauncherPresent();

// Returns true if the Chrome browser is installed (system-level or user-level).
// If this is running in an official build, it will check if a non-canary build
// if installed. If it is not an official build, it will always return true.
bool IsChromeBrowserPresent();

}  // namespace chrome_launcher_support

#endif  // CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_
