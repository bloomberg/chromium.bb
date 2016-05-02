// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines all install related constants that need to be used by Chrome as
// well as Chrome Installer.

#ifndef CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_

#include <stddef.h>

namespace installer {

// Return status of installer. Values in this enum must not change. Always add
// to the end. When removing an unused value, retain the deprecated name and
// value in a comment for posterity's sake, but take the liberty of removing the
// old doc string.
enum InstallStatus {
  FIRST_INSTALL_SUCCESS = 0,  // First install of Chrome succeeded.
  INSTALL_REPAIRED      = 1,  // Same version reinstalled for repair.
  NEW_VERSION_UPDATED   = 2,  // Chrome successfully updated to new version.
  EXISTING_VERSION_LAUNCHED = 3,  // No work done; launched existing Chrome.
  HIGHER_VERSION_EXISTS = 4,  // Higher version of Chrome already exists
  USER_LEVEL_INSTALL_EXISTS = 5,  // User level install already exists.
  SYSTEM_LEVEL_INSTALL_EXISTS = 6,  // Machine level install already exists.
  INSTALL_FAILED        = 7,  // Install/update failed.
  SETUP_PATCH_FAILED    = 8,  // Failed to patch setup.exe.
  OS_NOT_SUPPORTED      = 9,  // Current OS not supported.
  OS_ERROR             = 10,  // OS API call failed.
  TEMP_DIR_FAILED      = 11,  // Unable to get Temp directory.
  UNCOMPRESSION_FAILED = 12,  // Failed to uncompress Chrome archive.
  INVALID_ARCHIVE      = 13,  // Something wrong with the installer archive.
  INSUFFICIENT_RIGHTS  = 14,  // User trying system level install is not Admin.
  CHROME_NOT_INSTALLED = 15,  // Chrome not installed (returned in case of
                              // uninstall).
  CHROME_RUNNING       = 16,  // Chrome currently running (when trying to
                              // uninstall).
  UNINSTALL_CONFIRMED  = 17,  // User has confirmed Chrome uninstall.
  UNINSTALL_DELETE_PROFILE = 18,  // User okayed uninstall and profile deletion.
  UNINSTALL_SUCCESSFUL = 19,  // Chrome successfully uninstalled.
  UNINSTALL_FAILED     = 20,  // Chrome uninstallation failed.
  UNINSTALL_CANCELLED  = 21,  // User cancelled Chrome uninstallation.
  UNKNOWN_STATUS       = 22,  // Unknown status (this should never happen).
  RENAME_SUCCESSFUL    = 23,  // Rename of new_chrome.exe to chrome.exe worked.
  RENAME_FAILED        = 24,  // Rename of new_chrome.exe failed.
  EULA_REJECTED        = 25,  // EULA dialog was not accepted by user.
  EULA_ACCEPTED        = 26,  // EULA dialog was accepted by user.
  EULA_ACCEPTED_OPT_IN = 27,  // EULA accepted with the crash option selected.
  INSTALL_DIR_IN_USE   = 28,  // Installation directory is in use by another
                              // process
  UNINSTALL_REQUIRES_REBOOT = 29,  // Uninstallation required a reboot.
  IN_USE_UPDATED       = 30,  // Chrome successfully updated but old version
                              // running.
  SAME_VERSION_REPAIR_FAILED = 31,  // Chrome repair failed as Chrome was
                                    // running.
  REENTRY_SYS_UPDATE   = 32,  // Setup has been re-launched as the interactive
                              // user.
  SXS_OPTION_NOT_SUPPORTED = 33,  // The chrome-sxs option provided does not
                                  // work with other command line options.
  // NON_MULTI_INSTALLATION_EXISTS = 34,
  // MULTI_INSTALLATION_EXISTS = 35,
  // READY_MODE_OPT_IN_FAILED = 36,
  // READY_MODE_TEMP_OPT_OUT_FAILED = 37,
  // READY_MODE_END_TEMP_OPT_OUT_FAILED = 38,
  // CONFLICTING_CHANNEL_EXISTS = 39,
  // READY_MODE_REQUIRES_CHROME = 40,
  // APP_HOST_REQUIRES_MULTI_INSTALL = 41,
  APPLY_DIFF_PATCH_FAILED = 42,  // Failed to apply a diff patch.
  // INCONSISTENT_UPDATE_POLICY = 43,
  // APP_HOST_REQUIRES_USER_LEVEL = 44,
  // APP_HOST_REQUIRES_BINARIES = 45,
  // INSTALL_OF_GOOGLE_UPDATE_FAILED = 46,
  INVALID_STATE_FOR_OPTION = 47,  // A non-install option was called with an
                                  // invalid installer state.
  // WAIT_FOR_EXISTING_FAILED = 48,
  PATCH_INVALID_ARGUMENTS = 49,  // The arguments of --patch were missing or
                                 // they were invalid for any reason.
  DIFF_PATCH_SOURCE_MISSING = 50,  // No previous version archive found for
                                   // differential update.
  UNUSED_BINARIES      = 51,  // No multi-install products to update. The
                              // binaries will be uninstalled if they are not
                              // in use.
  UNUSED_BINARIES_UNINSTALLED = 52,  // The binaries were uninstalled.
  UNSUPPORTED_OPTION   = 53,  // An unsupported legacy option was given.
  CPU_NOT_SUPPORTED    = 54,  // Current OS not supported
  REENABLE_UPDATES_SUCCEEDED = 55,  // Autoupdates are now enabled.
  REENABLE_UPDATES_FAILED = 56,  // Autoupdates could not be enabled.
  UNPACKING_FAILED     = 57,  // Unpacking the (possibly patched) uncompressed
                              // archive failed.
  IN_USE_DOWNGRADE     = 58,  // Successfully downgrade chrome but current
                              // version is still running.
  OLD_VERSION_DOWNGRADE = 59,  // Successfully downgrade chrome to an older
                               // version.

  MAX_INSTALL_STATUS   = 60,  // Bump this out to make space for new results.
};

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
  NO_STAGE = 0,                    // No stage to report.
  PRECONDITIONS = 1,               // Evaluating pre-install conditions.
  UNCOMPRESSING = 2,               // Uncompressing chrome.packed.7z.
  ENSEMBLE_PATCHING = 3,           // Patching chrome.7z using courgette.
  BINARY_PATCHING = 4,             // Patching chrome.7z using bspatch.
  UNPACKING = 5,                   // Unpacking chrome.7z.
  BUILDING = 6,                    // Building the install work item list.
  EXECUTING = 7,                   // Executing the install work item list.
  ROLLINGBACK = 8,                 // Rolling-back the install work item list.
  REFRESHING_POLICY = 9,           // Refreshing the elevation policy.
  UPDATING_CHANNELS = 10,          // Updating channel information.
  COPYING_PREFERENCES_FILE = 11,   // Copying preferences file.
  CREATING_SHORTCUTS = 12,         // Creating shortcuts.
  REGISTERING_CHROME = 13,         // Performing Chrome registration.
  REMOVING_OLD_VERSIONS = 14,      // Deleting old version directories.
  FINISHING = 15,                  // Finishing the install.
  // CONFIGURE_AUTO_LAUNCH = 16,
  CREATING_VISUAL_MANIFEST = 17,   // Creating VisualElementsManifest.xml
  // DEFERRING_TO_HIGHER_VERSION = 18,
  UNINSTALLING_BINARIES = 19,      // Uninstalling unused binaries.
  UNINSTALLING_CHROME_FRAME = 20,  // Uninstalling multi-install Chrome Frame.
  NUM_STAGES                       // The number of stages.
};

// When we start reporting the numerical values from the enum, the order
// above MUST be preserved.
static_assert(UNINSTALLING_CHROME_FRAME == 20,
              "Never ever ever change InstallerStage values!");

namespace switches {

extern const char kChrome[];
extern const char kChromeFrame[];
extern const char kChromeSxS[];
extern const char kConfigureUserSettings[];
extern const char kCriticalUpdateVersion[];
extern const char kDeleteProfile[];
extern const char kDisableLogging[];
extern const char kDoNotLaunchChrome[];
extern const char kDoNotRegisterForUpdateLaunch[];
extern const char kDoNotRemoveSharedItems[];
extern const char kEnableLogging[];
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
extern const char kPreviousVersion[];
extern const char kReenableAutoupdates[];
extern const char kRegisterChromeBrowser[];
extern const char kRegisterChromeBrowserSuffix[];
extern const char kRegisterDevChrome[];
extern const char kRegisterURLProtocol[];
extern const char kRenameChromeExe[];
extern const char kRemoveChromeRegistration[];
extern const char kRunAsAdmin[];
extern const char kSelfDestruct[];
extern const char kSystemLevel[];
extern const char kTriggerActiveSetup[];
extern const char kUninstall[];
extern const char kUpdateSetupExe[];
extern const char kUncompressedArchive[];
extern const char kVerboseLogging[];
extern const char kShowEula[];
extern const char kShowEulaForMetro[];
extern const char kInactiveUserToast[];
extern const char kSystemLevelToast[];
extern const char kExperimentGroup[];
extern const char kToastResultsKey[];
extern const char kPatch[];
extern const char kInputFile[];
extern const char kPatchFile[];
extern const char kOutputFile[];

}  // namespace switches

namespace env_vars {

extern const char kGoogleUpdateIsMachineEnvVar[];

}  // namespace env_vars

extern const wchar_t kActiveSetupExe[];
extern const wchar_t kAppLauncherGuid[];
extern const wchar_t kChromeDll[];
extern const wchar_t kChromeChildDll[];
extern const wchar_t kChromeExe[];
extern const wchar_t kChromeFrameDll[];
extern const wchar_t kChromeFrameHelperDll[];
extern const wchar_t kChromeFrameHelperExe[];
extern const wchar_t kChromeFrameHelperWndClass[];
extern const wchar_t kChromeLauncherExe[];
extern const wchar_t kChromeNewExe[];
extern const wchar_t kChromeOldExe[];
extern const wchar_t kCmdOnOsUpgrade[];
extern const wchar_t kCmdQuickEnableCf[];
extern const wchar_t kEULASentinelFile[];
extern const wchar_t kGoogleChromeInstallSubDir1[];
extern const wchar_t kGoogleChromeInstallSubDir2[];
extern const wchar_t kInstallBinaryDir[];
extern const wchar_t kInstallerDir[];
extern const wchar_t kInstallTempDir[];
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

// Product options.
extern const wchar_t kOptionMultiInstall[];

// Chrome channel display names.
// NOTE: Canary is not strictly a 'channel', but rather a separate product
//     installed side-by-side. However, GoogleUpdateSettings::GetChromeChannel
//     will return "canary" for that product.
extern const wchar_t kChromeChannelUnknown[];
extern const wchar_t kChromeChannelCanary[];
extern const wchar_t kChromeChannelDev[];
extern const wchar_t kChromeChannelBeta[];
extern const wchar_t kChromeChannelStable[];
extern const wchar_t kChromeChannelStableExplicit[];

extern const size_t kMaxAppModelIdLength;

// The range of error values for the installer, Courgette, and bsdiff is
// overlapping. These offset values disambiguate between different sets
// of errors by shifting the values up with the specified offset.
const int kCourgetteErrorOffset = 300;
const int kBsdiffErrorOffset = 600;

// Arguments to --patch switch
extern const char kCourgette[];
extern const char kBsdiff[];

// Name of the allocator (and associated file) for storing histograms to be
// reported by Chrome during its next upload.
extern const char kSetupHistogramAllocatorName[];

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
