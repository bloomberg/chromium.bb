// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/util_constants.h"

namespace installer_util {

namespace switches {

// Run the installer in Chrome Frame mode.
const wchar_t kChromeFrame[] = L"chrome-frame";

// Run the installer for Chrome SxS.
const wchar_t kChromeSxS[] = L"chrome-sxs";

// Create Desktop and QuickLaunch shortcuts
const wchar_t kCreateAllShortcuts[] = L"create-all-shortcuts";

// Delete user profile data. This param is useful only when specified with
// kUninstall, otherwise it is silently ignored.
const wchar_t kDeleteProfile[] = L"delete-profile";

// Disable logging
const wchar_t kDisableLogging[] = L"disable-logging";

// Prevent installer from creating desktop shortcuts.
const wchar_t kDoNotCreateShortcuts[] = L"do-not-create-shortcuts";

// Prevent installer from launching Chrome after a successful first install.
const wchar_t kDoNotLaunchChrome[] = L"do-not-launch-chrome";

// Prevents installer from writing the Google Update key that causes Google
// Update to launch Chrome after a first install.
const wchar_t kDoNotRegisterForUpdateLaunch[] =
    L"do-not-register-for-update-launch";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
const wchar_t kDoNotRemoveSharedItems[] = L"do-not-remove-shared-items";

// Enable logging at the error level. This is the default behavior.
const wchar_t kEnableLogging[] = L"enable-logging";

// If present, setup will uninstall chrome without asking for any
// confirmation from user.
const wchar_t kForceUninstall[] = L"force-uninstall";

// Specify the file path of Chrome archive for install.
const char kInstallArchive[] = "install-archive";

// Specify the file path of Chrome master preference file.
const char kInstallerData[] = "installerdata";

// If present, specify file path to write logging info.
const wchar_t kLogFile[] = L"log-file";

// Register Chrome as default browser on the system. Usually this will require
// that setup is running as admin. If running as admin we try to register
// as default browser at system level, if running as non-admin we try to
// register as default browser only for the current user.
const wchar_t kMakeChromeDefault[] = L"make-chrome-default";

// Tells installer to expect to be run as a subsidiary to an MSI.
const wchar_t kMsi[] = L"msi";

// Useful only when used with --update-setup-exe, otherwise ignored. It
// specifies the full path where updated setup.exe will be stored.
const char kNewSetupExe[] = "new-setup-exe";

// Register Chrome as a valid browser on the current sytem. This option
// requires that setup.exe is running as admin. If this option is specified,
// options kInstallArchive and kUninstall are ignored.
const char kRegisterChromeBrowser[] = "register-chrome-browser";

const char kRegisterChromeBrowserSuffix[] =
    "register-chrome-browser-suffix";

// Renames chrome.exe to old_chrome.exe and renames new_chrome.exe to chrome.exe
// to support in-use updates. Also deletes opv key.
const wchar_t kRenameChromeExe[] = L"rename-chrome-exe";

// Removes Chrome registration from current machine. Requires admin rights.
const wchar_t kRemoveChromeRegistration[] = L"remove-chrome-registration";

// When we try to relaunch setup.exe as admin on Vista, we append this command
// line flag so that we try the launch only once.
const wchar_t kRunAsAdmin[] = L"run-as-admin";

// Install Chrome to system wise location. The default is per user install.
const wchar_t kSystemLevel[] = L"system-level";

// If present, setup will uninstall chrome.
const wchar_t kUninstall[] = L"uninstall";

// Also see --new-setup-exe. This command line option specifies a diff patch
// that setup.exe will apply to itself and store the resulting binary in the
// path given by --new-setup-exe.
const char kUpdateSetupExe[] = "update-setup-exe";

// Enable verbose logging (info level).
const wchar_t kVerboseLogging[] = L"verbose-logging";

// Show the embedded EULA dialog.
const char kShowEula[] = "show-eula";

// Use the alternate desktop shortcut name.
const wchar_t kAltDesktopShortcut[] = L"alt-desktop-shortcut";

// Perform the inactive user toast experiment.
const char kInactiveUserToast[] = "inactive-user-toast";

// User toast experiment switch from system context to user context.
const wchar_t kSystemLevelToast[] = L"system-level-toast";

}  // namespace switches

const wchar_t kGoogleChromeInstallSubDir1[] = L"Google";
const wchar_t kGoogleChromeInstallSubDir2[] = L"Chrome";
const wchar_t kInstallBinaryDir[] = L"Application";
const wchar_t kInstallUserDataDir[] = L"User Data";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kChromeOldExe[] = L"old_chrome.exe";
const wchar_t kChromeNewExe[] = L"new_chrome.exe";
const wchar_t kNaClExe[] = L"nacl64.exe";
const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kChromeNaCl64Dll[] = L"nacl64.dll";
const wchar_t kChromeFrameDll[] = L"npchrome_frame.dll";
const wchar_t kSetupExe[] = L"setup.exe";
const wchar_t kInstallerDir[] = L"Installer";
const wchar_t kSxSSuffix[] = L" SxS";

const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kUninstallDisplayNameField[] = L"DisplayName";
const char kUninstallMetricsName[] = "uninstall_metrics";
const wchar_t kUninstallInstallationDate[] = L"installation_date";
}  // namespace installer_util
