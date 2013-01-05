// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines all install related constants that need to be used by Chrome as
// well as Chrome Installer.

#ifndef CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_

#include "base/basictypes.h"

namespace installer {

// Return status of installer
enum InstallStatus {
  FIRST_INSTALL_SUCCESS,  // 0. Successfully installed Chrome for the first time
  INSTALL_REPAIRED,       // 1. Same version reinstalled for repair
  NEW_VERSION_UPDATED,    // 2. Chrome successfully updated to new version
  EXISTING_VERSION_LAUNCHED,  // 3. No work done, just launched existing chrome
  HIGHER_VERSION_EXISTS,  // 4. Higher version of Chrome already exists
  USER_LEVEL_INSTALL_EXISTS,  // 5. User level install already exists
  SYSTEM_LEVEL_INSTALL_EXISTS,  // 6. Machine level install already exists
  INSTALL_FAILED,         // 7. Install/update failed
  SETUP_PATCH_FAILED,     // 8. Failed to patch setup.exe
  OS_NOT_SUPPORTED,       // 9. Current OS not supported
  OS_ERROR,               // 10. OS API call failed
  TEMP_DIR_FAILED,        // 11. Unable to get Temp directory
  UNCOMPRESSION_FAILED,   // 12. Failed to uncompress Chrome archive
  INVALID_ARCHIVE,        // 13. Something wrong with the installer archive
  INSUFFICIENT_RIGHTS,    // 14. User trying system level install is not Admin
  CHROME_NOT_INSTALLED,   // 15. Chrome not installed (returned in case of
                          // uninstall)
  CHROME_RUNNING,         // 16. Chrome currently running (when trying to
                          // uninstall)
  UNINSTALL_CONFIRMED,    // 17. User has confirmed Chrome uninstall
  UNINSTALL_DELETE_PROFILE,  // 18. User okayed uninstall and profile deletion.
  UNINSTALL_SUCCESSFUL,   // 19. Chrome successfully uninstalled
  UNINSTALL_FAILED,       // 20. Chrome uninstallation failed
  UNINSTALL_CANCELLED,    // 21. User cancelled Chrome uninstallation
  UNKNOWN_STATUS,         // 22. Unknown status (this should never happen)
  RENAME_SUCCESSFUL,      // 23. Rename of new_chrome.exe to chrome.exe worked
  RENAME_FAILED,          // 24. Rename of new_chrome.exe failed
  EULA_REJECTED,          // 25. EULA dialog was not accepted by user.
  EULA_ACCEPTED,          // 26. EULA dialog was accepted by user.
  EULA_ACCEPTED_OPT_IN,   // 27. EULA accepted with the crash option selected.
  INSTALL_DIR_IN_USE,     // 28. Installation directory is in use by another
                          // process
  UNINSTALL_REQUIRES_REBOOT,  // 29. Uninstallation required a reboot.
  IN_USE_UPDATED,         // 30. Chrome successfully updated but old version
                          // running
  SAME_VERSION_REPAIR_FAILED,  // 31. Chrome repair failed as Chrome was running
  REENTRY_SYS_UPDATE,     // 32. Setup has been re-launched as the interactive
                          // user
  SXS_OPTION_NOT_SUPPORTED,  // 33. The chrome-sxs option provided does not work
                             // with other command line options.
  NON_MULTI_INSTALLATION_EXISTS,  // 34. We tried to do a multi-install but
                                  // failed because there's an existing
                                  // installation of the same product on the
                                  // system, but in 'single' mode.
  MULTI_INSTALLATION_EXISTS,  // 35. We tried to do a 'single' install but
                              // failed because there's an existing
                              // multi-install installation of the same product
                              // on the system.
  READY_MODE_OPT_IN_FAILED,   // 36. Failed to opt-into Chrome Frame.
  READY_MODE_TEMP_OPT_OUT_FAILED,   // 37. Failed to temporarily opt-out of
                                    // Chrome Frame.
  READY_MODE_END_TEMP_OPT_OUT_FAILED,   // 38. Failed to end temporary opt-out
                                        // of Chrome Frame.
  CONFLICTING_CHANNEL_EXISTS,  // 39. A multi-install product on a different
                               // update channel exists.
  READY_MODE_REQUIRES_CHROME,  // 40. Chrome Frame in ready-mode requires Chrome
  APP_HOST_REQUIRES_MULTI_INSTALL,  // 41. --multi-install was missing from the
                               // command line.
  APPLY_DIFF_PATCH_FAILED,     // 42. Failed to apply a diff patch.
  INCONSISTENT_UPDATE_POLICY,  // 43. Inconsistent update policy GP settings.
  APP_HOST_REQUIRES_USER_LEVEL,  // 44. --system-level is forbidden.
  APP_HOST_REQUIRES_BINARIES,  // 45. No Chrome binaries at either level.
  INSTALL_OF_GOOGLE_UPDATE_FAILED,  // 46. Failed to install Google Update.
  INVALID_STATE_FOR_OPTION,    // 47. A non-install option was called with an
                               // invalid installer state.
  WAIT_FOR_EXISTING_FAILED,    // 48. OS error waiting for existing setup.exe.
  // Friendly reminder: note the COMPILE_ASSERT below.
};


// Existing InstallStatus values must not change.  Always add to the end.
COMPILE_ASSERT(installer::WAIT_FOR_EXISTING_FAILED == 48,
               dont_change_enum);

// The type of an update archive.
enum ArchiveType {
  UNKNOWN_ARCHIVE_TYPE,     // Unknown or uninitialized.
  FULL_ARCHIVE_TYPE,        // Full chrome.7z archive.
  INCREMENTAL_ARCHIVE_TYPE  // Incremental or differential archive.
};

// Stages of an installation reported through Google Update on failure.
// The order and value of existing enums must not change. Please add new
// values to the end (before NUM_STAGES) and update the compile assert below
// to assert on the last value added.
enum InstallerStage {
  NO_STAGE,                    // 0: No stage to report.
  PRECONDITIONS,               // 1: Evaluating pre-install conditions.
  UNCOMPRESSING,               // 2: Uncompressing chrome.packed.7z.
  ENSEMBLE_PATCHING,           // 3: Patching chrome.7z using courgette.
  BINARY_PATCHING,             // 4: Patching chrome.7z using bspatch.
  UNPACKING,                   // 5: Unpacking chrome.7z.
  BUILDING,                    // 6: Building the install work item list.
  EXECUTING,                   // 7: Executing the install work item list.
  ROLLINGBACK,                 // 8: Rolling-back the install work item list.
  REFRESHING_POLICY,           // 9: Refreshing the elevation policy.
  UPDATING_CHANNELS,           // 10: Updating channel information.
  COPYING_PREFERENCES_FILE,    // 11: Copying preferences file.
  CREATING_SHORTCUTS,          // 12: Creating shortcuts.
  REGISTERING_CHROME,          // 13: Performing Chrome registration.
  REMOVING_OLD_VERSIONS,       // 14: Deleting old version directories.
  FINISHING,                   // 15: Finishing the install.
  CONFIGURE_AUTO_LAUNCH,       // 16: Configuring Chrome to auto-launch.
  CREATING_VISUAL_MANIFEST,    // 17: Creating VisualElementsManifest.xml
  DEFERRING_TO_HIGHER_VERSION,  // 18: Deferring to an installed higher version.
  NUM_STAGES                   // 19: The number of stages.
};

// When we start reporting the numerical values from the enum, the order
// above MUST be preserved.
COMPILE_ASSERT(DEFERRING_TO_HIGHER_VERSION == 18,
               never_ever_ever_change_InstallerStage_values_bang);

namespace switches {
extern const char kAutoLaunchChrome[];
extern const char kChrome[];
extern const char kChromeAppHost[];
extern const char kChromeAppLauncher[];
extern const char kChromeFrame[];
extern const char kChromeFrameQuickEnable[];
extern const char kChromeFrameReadyMode[];
extern const char kChromeFrameReadyModeOptIn[];
extern const char kChromeFrameReadyModeTempOptOut[];
extern const char kChromeFrameReadyModeEndTempOptOut[];
extern const char kChromeSxS[];
extern const char kConfigureUserSettings[];
extern const char kCriticalUpdateVersion[];
extern const char kDeleteProfile[];
extern const char kDisableLogging[];
extern const char kDoNotLaunchChrome[];
extern const char kDoNotRegisterForUpdateLaunch[];
extern const char kDoNotRemoveSharedItems[];
extern const char kEnableLogging[];
extern const char kEnsureGoogleUpdatePresent[];
extern const char kForceConfigureUserSettings[];
extern const char kForceUninstall[];
extern const char kInstallArchive[];
extern const char kInstallerData[];
extern const char kLogFile[];
extern const char kMakeChromeDefault[];
extern const char kMsi[];
extern const char kMultiInstall[];
extern const char kNewSetupExe[];
extern const char kOnOsUpgrade[];
extern const char kRegisterChromeBrowser[];
extern const char kRegisterChromeBrowserSuffix[];
extern const char kRegisterDevChrome[];
extern const char kRegisterURLProtocol[];
extern const char kRenameChromeExe[];
extern const char kRemoveChromeRegistration[];
extern const char kRunAsAdmin[];
extern const char kSelfDestruct[];
extern const char kSystemLevel[];
extern const char kUninstall[];
extern const char kUpdateSetupExe[];
extern const char kVerboseLogging[];
extern const char kShowEula[];
extern const char kShowEulaForMetro[];
extern const char kInactiveUserToast[];
extern const char kSystemLevelToast[];
extern const char kExperimentGroup[];
extern const char kToastResultsKey[];
}  // namespace switches

extern const wchar_t kChromeAppHostExe[];
extern const wchar_t kChromeDll[];
extern const wchar_t kChromeExe[];
extern const wchar_t kChromeFrameDll[];
extern const wchar_t kChromeFrameHelperExe[];
extern const wchar_t kChromeFrameHelperWndClass[];
extern const wchar_t kChromeFrameReadyModeField[];
extern const wchar_t kChromeLauncherExe[];
extern const wchar_t kChromeOldExe[];
extern const wchar_t kChromeNewExe[];
extern const wchar_t kCmdInstallApp[];
extern const wchar_t kCmdOnOsUpgrade[];
extern const wchar_t kCmdQuickEnableApplicationHost[];
extern const wchar_t kCmdQuickEnableCf[];
extern const wchar_t kDelegateExecuteExe[];
extern const wchar_t kEULASentinelFile[];
extern const wchar_t kGoogleChromeInstallSubDir1[];
extern const wchar_t kGoogleChromeInstallSubDir2[];
extern const wchar_t kInstallBinaryDir[];
extern const wchar_t kInstallerDir[];
extern const wchar_t kInstallTempDir[];
extern const wchar_t kInstallUserDataDir[];
extern const wchar_t kLnkExt[];
extern const wchar_t kNaClExe[];
extern const wchar_t kSetupExe[];
extern const wchar_t kSxSSuffix[];
extern const wchar_t kUninstallArgumentsField[];
extern const wchar_t kUninstallDisplayNameField[];
extern const wchar_t kUninstallInstallationDate[];
extern const char kUninstallMetricsName[];
extern const wchar_t kUninstallStringField[];

// Google Update installer result API
extern const wchar_t kInstallerError[];
extern const wchar_t kInstallerExtraCode1[];
extern const wchar_t kInstallerResult[];
extern const wchar_t kInstallerResultUIString[];
extern const wchar_t kInstallerSuccessLaunchCmdLine[];

// Google Update named environment variable that implies kSystemLevel.
extern const char kGoogleUpdateIsMachineEnvVar[];

// Product options.
extern const wchar_t kOptionMultiInstall[];
extern const wchar_t kOptionReadyMode[];
extern const wchar_t kOptionAppHostIsLauncher[];

// Chrome channel display names.
// NOTE: Canary is not strictly a 'channel', but rather a separate product
//     installed side-by-side. However, GoogleUpdateSettings::GetChromeChannel
//     will return "canary" for that product.
extern const wchar_t kChromeChannelUnknown[];
extern const wchar_t kChromeChannelCanary[];
extern const wchar_t kChromeChannelDev[];
extern const wchar_t kChromeChannelBeta[];
extern const wchar_t kChromeChannelStable[];

extern const size_t kMaxAppModelIdLength;

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
