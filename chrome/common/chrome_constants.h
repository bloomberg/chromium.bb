// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_

#include "base/files/file_path.h"

namespace chrome {

extern const char kChromeVersion[];

extern const char kChromeVersionEnvVar[];

extern const base::FilePath::CharType kBrowserProcessExecutableName[];
extern const base::FilePath::CharType kHelperProcessExecutableName[];
extern const base::FilePath::CharType kBrowserProcessExecutablePath[];
extern const base::FilePath::CharType kHelperProcessExecutablePath[];
extern const base::FilePath::CharType kBrowserProcessExecutableNameChromium[];
extern const base::FilePath::CharType kHelperProcessExecutableNameChromium[];
extern const base::FilePath::CharType kBrowserProcessExecutablePathChromium[];
extern const base::FilePath::CharType kHelperProcessExecutablePathChromium[];
#if defined(OS_MACOSX)
// NOTE: if you change the value of kFrameworkName, please don't forget to
// update components/test/run_all_unittests.cc as well.
// TODO(tfarina): Remove the comment above, when you fix components to use plist
// on Mac.
extern const base::FilePath::CharType kFrameworkName[];

// The helper .app bundle name and executable name may have one of these
// suffixes to identify specific features, or it may have no suffix at all.
// This is a NULL-terminated array of strings. If kHelperFlavorSuffixes
// contains "EN", "MF", and NULL, it indicates that if the normal helper is
// named Chromium Helper.app, helper executables could show up at any of
// Chromium Helper.app/Contents/MacOS/Chromium Helper,
// Chromium Helper EN.app/Contents/MacOS/Chromium Helper EN, and
// Chromium Helper MF.app/Contents/MacOS/Chromium Helper MF.
extern const base::FilePath::CharType* const kHelperFlavorSuffixes[];
#endif  // OS_MACOSX
#if defined(OS_WIN)
extern const base::FilePath::CharType kMetroDriverDll[];
extern const wchar_t kStatusTrayWindowClass[];
#endif  // defined(OS_WIN)
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const char    kInitialProfile[];
extern const char    kMultiProfileDirPrefix[];
extern const base::FilePath::CharType kGuestProfileDir[];
extern const wchar_t kBrowserResourcesDll[];

// filenames
#if defined(OS_ANDROID)
extern const base::FilePath::CharType kAndroidCacheFilename[];
#endif
extern const base::FilePath::CharType kArchivedHistoryFilename[];
extern const base::FilePath::CharType kCacheDirname[];
extern const base::FilePath::CharType kCookieFilename[];
extern const base::FilePath::CharType kCRLSetFilename[];
extern const base::FilePath::CharType kCustomDictionaryFileName[];
extern const base::FilePath::CharType kExtensionActivityLogFilename[];
extern const base::FilePath::CharType kExtensionsCookieFilename[];
extern const base::FilePath::CharType kFaviconsFilename[];
extern const base::FilePath::CharType kFirstRunSentinel[];
extern const base::FilePath::CharType kGCMStoreDirname[];
extern const base::FilePath::CharType kHistoryFilename[];
extern const base::FilePath::CharType kJumpListIconDirname[];
extern const base::FilePath::CharType kLocalStateFilename[];
extern const base::FilePath::CharType kLocalStorePoolName[];
extern const base::FilePath::CharType kLoginDataFileName[];
extern const base::FilePath::CharType kMediaCacheDirname[];
extern const base::FilePath::CharType kNewTabThumbnailsFilename[];
extern const base::FilePath::CharType kOBCertFilename[];
extern const base::FilePath::CharType kPreferencesFilename[];
extern const base::FilePath::CharType kProtectedPreferencesFilenameDeprecated[];
extern const base::FilePath::CharType kReadmeFilename[];
extern const base::FilePath::CharType kResetPromptMementoFilename[];
extern const base::FilePath::CharType kSafeBrowsingBaseFilename[];
extern const base::FilePath::CharType kSecurePreferencesFilename[];
extern const base::FilePath::CharType kServiceStateFileName[];
extern const base::FilePath::CharType kShortcutsDatabaseName[];
extern const base::FilePath::CharType kSingletonCookieFilename[];
extern const base::FilePath::CharType kSingletonLockFilename[];
extern const base::FilePath::CharType kSingletonSocketFilename[];
extern const base::FilePath::CharType kSupervisedUserSettingsFilename[];
extern const base::FilePath::CharType kSyncCredentialsFilename[];
extern const base::FilePath::CharType kThemePackFilename[];
extern const base::FilePath::CharType kThumbnailsFilename[];
extern const base::FilePath::CharType kTopSitesFilename[];
extern const base::FilePath::CharType kWebAppDirname[];

// File name of the Pepper Flash plugin on different platforms.
extern const base::FilePath::CharType kPepperFlashPluginFilename[];

// directory names
extern const wchar_t kUserDataDirname[];

extern const bool kRecordModeEnabled;

// If a WebContents is impolite and displays a second JavaScript alert within
// kJavaScriptMessageExpectedDelay of a previous JavaScript alert being
// dismissed, display an option to suppress future alerts from this WebContents.
extern const int kJavaScriptMessageExpectedDelay;

// Are touch icons enabled? False by default.
extern const bool kEnableTouchIcon;

// Fraction of the total number of processes to be used for hosting
// extensions. If we have more extensions than this percentage, we will start
// combining extensions in existing processes. This allows web pages to have
// enough render processes and not be starved when a lot of extensions are
// installed.
extern const float kMaxShareOfExtensionProcesses;

// This is used by the PreRead experiment.
extern const char kPreReadEnvironmentVariable[];

#if defined(OS_LINUX)
// The highest and lowest assigned OOM score adjustment
// (oom_score_adj) used by the OomPriority Manager.
extern const int kLowestRendererOomScore;
extern const int kHighestRendererOomScore;
#endif

#if defined(OS_WIN)
// Used by Metro Chrome to initiate navigation and search requests.
extern const wchar_t kMetroNavigationAndSearchMessage[];
// Used by Metro Chrome to get information about the current tab.
extern const wchar_t kMetroGetCurrentTabInfoMessage[];
// Used by Metro Chrome to store activation state.
extern const wchar_t kMetroRegistryPath[];
extern const wchar_t kLaunchModeValue[];
// Used by the browser as a container in which to track unreported crash dump
// attempts. The actual values (each representing one crash dump attempt) are
// stored in a subkey named with the version number of the build. Each value
// under the subkey represents an additional attempt.
extern const wchar_t kBrowserCrashDumpAttemptsRegistryPath[];
// Used by chrome.exe to signal that chrome.dll was started via a key sequence
// that requires it to start in safe mode. For example, in software rendering.
extern const char kSafeModeEnvVar[];
#endif

#if defined(OS_CHROMEOS)
// Chrome OS profile directories have custom prefix.
// Profile path format: [user_data_dir]/u-[$hash]
// Ex.: /home/chronos/u-0123456789
extern const char kProfileDirPrefix[];

// Legacy profile dir that was used when only one cryptohome has been mounted.
extern const char kLegacyProfileDir[];

// This must be kept in sync with TestingProfile::kTestUserProfileDir.
extern const char kTestUserProfileDir[];
#endif

// Used to identify the application to the system AV function in Windows.
extern const char kApplicationClientIDStringForAVScanning[];

#if defined(OS_ANDROID)
// The largest reasonable length we'd assume for a meta tag attribute.
extern const size_t kMaxMetaTagAttributeLength;
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
