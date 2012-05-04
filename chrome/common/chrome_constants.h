// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_
#pragma once

#include "base/file_path.h"

namespace chrome {

extern const char kChromeVersion[];

extern const char kChromeVersionEnvVar[];

extern const FilePath::CharType kBrowserProcessExecutableName[];
extern const FilePath::CharType kHelperProcessExecutableName[];
extern const FilePath::CharType kBrowserProcessExecutablePath[];
extern const FilePath::CharType kHelperProcessExecutablePath[];
extern const FilePath::CharType kBrowserProcessExecutableNameChromium[];
extern const FilePath::CharType kHelperProcessExecutableNameChromium[];
extern const FilePath::CharType kBrowserProcessExecutablePathChromium[];
extern const FilePath::CharType kHelperProcessExecutablePathChromium[];
#if defined(OS_MACOSX)
extern const FilePath::CharType kFrameworkName[];

// The helper .app bundle name and executable name may have one of these
// suffixes to identify specific features, or it may have no suffix at all.
// This is a NULL-terminated array of strings. If kHelperFlavorSuffixes
// contains "EN", "MF", and NULL, it indicates that if the normal helper is
// named Chromium Helper.app, helper executables could show up at any of
// Chromium Helper.app/Contents/MacOS/Chromium Helper,
// Chromium Helper EN.app/Contents/MacOS/Chromium Helper EN, and
// Chromium Helper MF.app/Contents/MacOS/Chromium Helper MF.
extern const FilePath::CharType* const kHelperFlavorSuffixes[];
#endif  // OS_MACOSX
extern const wchar_t kBrowserAppName[];
#if defined(OS_WIN)
extern const wchar_t kStatusTrayWindowClass[];
#endif  // defined(OS_WIN)
extern const wchar_t kMessageWindowClass[];
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const char    kInitialProfile[];
extern const char    kMultiProfileDirPrefix[];
extern const wchar_t kBrowserResourcesDll[];
extern const wchar_t kNaClAppName[];
extern const FilePath::CharType kExtensionFileExtension[];
extern const FilePath::CharType kExtensionKeyFileExtension[];

// filenames
#if defined(OS_ANDROID)
extern const FilePath::CharType kAndroidCacheFilename[];
#endif
extern const FilePath::CharType kArchivedHistoryFilename[];
extern const FilePath::CharType kCacheDirname[];
extern const FilePath::CharType kCRLSetFilename[];
extern const FilePath::CharType kMediaCacheDirname[];
extern const FilePath::CharType kOffTheRecordMediaCacheDirname[];
extern const FilePath::CharType kThemePackFilename[];
extern const FilePath::CharType kCookieFilename[];
extern const FilePath::CharType kOBCertFilename[];
extern const FilePath::CharType kExtensionsCookieFilename[];
extern const FilePath::CharType kIsolatedAppStateDirname[];
extern const FilePath::CharType kFaviconsFilename[];
extern const FilePath::CharType kHistoryFilename[];
extern const FilePath::CharType kLocalStateFilename[];
extern const FilePath::CharType kPreferencesFilename[];
extern const FilePath::CharType kSafeBrowsingBaseFilename[];
extern const FilePath::CharType kSingletonCookieFilename[];
extern const FilePath::CharType kSingletonSocketFilename[];
extern const FilePath::CharType kSingletonLockFilename[];
extern const FilePath::CharType kThumbnailsFilename[];
extern const FilePath::CharType kNewTabThumbnailsFilename[];
extern const FilePath::CharType kTopSitesFilename[];
extern const wchar_t kUserDataDirname[];
extern const FilePath::CharType kWebDataFilename[];
extern const FilePath::CharType kBookmarksFileName[];
extern const FilePath::CharType kHistoryBookmarksFileName[];
extern const FilePath::CharType kCustomDictionaryFileName[];
extern const FilePath::CharType kLoginDataFileName[];
extern const FilePath::CharType kJumpListIconDirname[];
extern const FilePath::CharType kWebAppDirname[];
extern const FilePath::CharType kServiceStateFileName[];
extern const FilePath::CharType kReadmeFilename[];

#if defined(OS_CHROMEOS)
extern const FilePath::CharType kGDataCacheDirname[];
#endif  // defined(OS_CHROMEOS)

// File name of the Pepper Flash plugin on different platforms.
extern const FilePath::CharType kPepperFlashPluginFilename[];

extern const bool kRecordModeEnabled;

// Most sequence numbers are used by a renderer when responding to a browser
// request for histogram data.  This reserved number is used when a renderer
// sends an unprovoked update, such as after a page has been loaded.  Using
// this reserved constant avoids any chance of confusion with a response having
// a browser-supplied sequence number.
extern const int kHistogramSynchronizerReservedSequenceNumber;

// The language code used when the language of a page could not be detected.
// (Matches what the CLD -Compact Language Detection- library reports.)
extern const char* const kUnknownLanguageCode;

// If another javascript message box is displayed within
// kJavascriptMessageExpectedDelay of a previous javascript message box being
// dismissed, display an option to suppress future message boxes from this
// contents.
extern const int kJavascriptMessageExpectedDelay;

// Are touch icons enabled? False by default.
extern const bool kEnableTouchIcon;

// Fraction of the total number of processes to be used for hosting
// extensions. If we have more extensions than this percentage, we will start
// combining extensions in existing processes. This allows web pages to have
// enough render processes and not be starved when a lot of extensions are
// installed.
extern const float kMaxShareOfExtensionProcesses;

#if defined(OS_LINUX)
// The highest and lowest assigned OOM score adjustment
// (oom_score_adj) used by the OomPriority Manager.
extern const int kLowestRendererOomScore;
extern const int kHighestRendererOomScore;
#endif

#if defined(OS_WIN)
// This is used by the PreRead experiment.
extern const char kPreReadEnvironmentVariable[];
// Used by Metro Chrome to create the profile under a custom subdirectory.
extern const wchar_t kMetroChromeUserDataSubDir[];
// Used by Metro Chrome to initiate navigation and search requests.
extern const wchar_t kMetroNavigationAndSearchMessage[];
// Used by Metro Chrome to get information about the current tab.
extern const wchar_t kMetroGetCurrentTabInfoMessage[];
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
