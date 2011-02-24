// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace switches {

// Install CEEE.
const char kCeee[] = "ceee";

// Install Chrome.
// Currently this is only required when used in combination with kMultiInstall.
const char kChrome[] = "chrome";

// Install Chrome Frame.
const char kChromeFrame[] = "chrome-frame";

// Installs Chrome Frame from an already installed multi-install of Chrome.
const char kChromeFrameQuickEnable[] = "quick-enable-cf";

// When installing Chrome Frame, install it in ready mode.
// If --chrome-frame is not on the command line, this switch has no effect.
const char kChromeFrameReadyMode[] = "ready-mode";

// GCF ready mode opt-in.  This enables a full installation of GCF.
const char kChromeFrameReadyModeOptIn[] = "ready-mode-opt-in";

// GCF ready mode temp opt-out.  This disables the GCF user agent modification
// and detection of headers/meta tags.
const char kChromeFrameReadyModeTempOptOut[] = "ready-mode-temp-opt-out";

// End GCF ready mode temp opt-out.  This re-enables the GCF user agent
// modification and detection of headers/meta tags.
const char kChromeFrameReadyModeEndTempOptOut[] = "ready-mode-end-temp-opt-out";

// Run the installer for Chrome SxS.
const char kChromeSxS[] = "chrome-sxs";

// Create Desktop and QuickLaunch shortcuts
const char kCreateAllShortcuts[] = "create-all-shortcuts";

// Delete user profile data. This param is useful only when specified with
// kUninstall, otherwise it is silently ignored.
const char kDeleteProfile[] = "delete-profile";

// Disable logging
const char kDisableLogging[] = "disable-logging";

// Prevent installer from creating desktop shortcuts.
const char kDoNotCreateShortcuts[] = "do-not-create-shortcuts";

// Prevent installer from launching Chrome after a successful first install.
const char kDoNotLaunchChrome[] = "do-not-launch-chrome";

// Prevents installer from writing the Google Update key that causes Google
// Update to launch Chrome after a first install.
const char kDoNotRegisterForUpdateLaunch[] =
    "do-not-register-for-update-launch";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
const char kDoNotRemoveSharedItems[] = "do-not-remove-shared-items";

// Enable logging at the error level. This is the default behavior.
const char kEnableLogging[] = "enable-logging";

// If present, setup will uninstall chrome without asking for any
// confirmation from user.
const char kForceUninstall[] = "force-uninstall";

// Specify the file path of Chrome archive for install.
const char kInstallArchive[] = "install-archive";

// Specify the file path of Chrome master preference file.
const char kInstallerData[] = "installerdata";

// If present, specify file path to write logging info.
const char kLogFile[] = "log-file";

// Register Chrome as default browser on the system. Usually this will require
// that setup is running as admin. If running as admin we try to register
// as default browser at system level, if running as non-admin we try to
// register as default browser only for the current user.
const char kMakeChromeDefault[] = "make-chrome-default";

// Tells installer to expect to be run as a subsidiary to an MSI.
const char kMsi[] = "msi";

// Tells installer to install multiple products specified on the command line.
// (e.g. Chrome Frame, CEEE, Chrome)
const char kMultiInstall[] = "multi-install";

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
const char kRenameChromeExe[] = "rename-chrome-exe";

// Removes Chrome registration from current machine. Requires admin rights.
const char kRemoveChromeRegistration[] = "remove-chrome-registration";

// When we try to relaunch setup.exe as admin on Vista, we append this command
// line flag so that we try the launch only once.
const char kRunAsAdmin[] = "run-as-admin";

// Install Chrome to system wise location. The default is per user install.
const char kSystemLevel[] = "system-level";

// If present, setup will uninstall chrome.
const char kUninstall[] = "uninstall";

// Also see --new-setup-exe. This command line option specifies a diff patch
// that setup.exe will apply to itself and store the resulting binary in the
// path given by --new-setup-exe.
const char kUpdateSetupExe[] = "update-setup-exe";

// Enable verbose logging (info level).
const char kVerboseLogging[] = "verbose-logging";

// Show the embedded EULA dialog.
const char kShowEula[] = "show-eula";

// Use the alternate desktop shortcut name.
const char kAltDesktopShortcut[] = "alt-desktop-shortcut";

// Perform the inactive user toast experiment.
const char kInactiveUserToast[] = "inactive-user-toast";

// User toast experiment switch from system context to user context.
const char kSystemLevelToast[] = "system-level-toast";

// A handle value of the key to write the results of the toast experiment
// to. See DuplicateGoogleUpdateSystemClientKey for details.
const char kToastResultsKey[] = "toast-results-key";

}  // namespace switches

const wchar_t kCeeeBrokerExe[] = L"ceee_broker.exe";
const wchar_t kCeeeIeDll[] = L"ceee_ie.dll";
const wchar_t kCeeeInstallHelperDll[] = L"ceee_installer_helper.dll";
const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kChromeFrameDll[] = L"npchrome_frame.dll";
const wchar_t kChromeFrameHelperExe[] = L"chrome_frame_helper.exe";
const wchar_t kChromeFrameHelperWndClass[] = L"ChromeFrameHelperWindowClass";
const wchar_t kChromeFrameReadyModeField[] = L"ChromeFrameReadyMode";
const wchar_t kChromeLauncherExe[] = L"chrome_launcher.exe";
const wchar_t kChromeNaCl64Dll[] = L"nacl64.dll";
const wchar_t kChromeNewExe[] = L"new_chrome.exe";
const wchar_t kChromeOldExe[] = L"old_chrome.exe";
const wchar_t kGoogleChromeInstallSubDir1[] = L"Google";
const wchar_t kGoogleChromeInstallSubDir2[] = L"Chrome";
const wchar_t kInstallBinaryDir[] = L"Application";
const wchar_t kInstallerDir[] = L"Installer";
const wchar_t kInstallTempDir[] = L"Temp";
const wchar_t kInstallUserDataDir[] = L"User Data";
const wchar_t kNaClExe[] = L"nacl64.exe";
const wchar_t kSetupExe[] = L"setup.exe";
const wchar_t kSxSSuffix[] = L" SxS";
const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kUninstallDisplayNameField[] = L"DisplayName";
const char kUninstallMetricsName[] = "uninstall_metrics";
const wchar_t kUninstallInstallationDate[] = L"installation_date";
const wchar_t kInstallerResult[] = L"InstallerResult";
const wchar_t kInstallerError[] = L"InstallerError";
const wchar_t kInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

const wchar_t kOptionCeee[] = L"ceee";
const wchar_t kOptionMultiInstall[] = L"multi-install";
const wchar_t kOptionReadyMode[] = L"ready-mode";

}  // namespace installer
