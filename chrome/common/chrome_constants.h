// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#if defined(OS_MACOSX)
extern const FilePath::CharType kFrameworkName[];
#endif
extern const wchar_t kBrowserAppName[];
#if defined(OS_WIN)
extern const wchar_t kStatusTrayWindowClass[];
#endif  // defined(OS_WIN)
extern const wchar_t kMessageWindowClass[];
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const char    kNotSignedInProfile[];
extern const char    kStatsFilename[];
extern const wchar_t kBrowserResourcesDll[];
extern const wchar_t kNaClAppName[];
extern const FilePath::CharType kExtensionFileExtension[];
extern const FilePath::CharType kExtensionKeyFileExtension[];

// filenames
extern const FilePath::CharType kArchivedHistoryFilename[];
extern const FilePath::CharType kCacheDirname[];
extern const FilePath::CharType kMediaCacheDirname[];
extern const FilePath::CharType kOffTheRecordMediaCacheDirname[];
extern const FilePath::CharType kAppCacheDirname[];
extern const FilePath::CharType kThemePackFilename[];
extern const FilePath::CharType kCookieFilename[];
extern const FilePath::CharType kExtensionsCookieFilename[];
extern const FilePath::CharType kFaviconsFilename[];
extern const FilePath::CharType kHistoryFilename[];
extern const FilePath::CharType kLocalStateFilename[];
extern const FilePath::CharType kPreferencesFilename[];
extern const FilePath::CharType kSafeBrowsingBaseFilename[];
extern const FilePath::CharType kSafeBrowsingPhishingModelFilename[];
extern const FilePath::CharType kSingletonCookieFilename[];
extern const FilePath::CharType kSingletonSocketFilename[];
extern const FilePath::CharType kSingletonLockFilename[];
extern const FilePath::CharType kThumbnailsFilename[];
extern const FilePath::CharType kNewTabThumbnailsFilename[];
extern const FilePath::CharType kTopSitesFilename[];
extern const wchar_t kUserDataDirname[];
extern const FilePath::CharType kUserScriptsDirname[];
extern const FilePath::CharType kWebDataFilename[];
extern const FilePath::CharType kBookmarksFileName[];
extern const FilePath::CharType kHistoryBookmarksFileName[];
extern const FilePath::CharType kCustomDictionaryFileName[];
extern const FilePath::CharType kLoginDataFileName[];
extern const FilePath::CharType kJumpListIconDirname[];
extern const FilePath::CharType kWebAppDirname[];
extern const FilePath::CharType kServiceStateFileName[];

extern const unsigned int kMaxRendererProcessCount;
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

// The maximum number of characters of the document's title that we're willing
// to accept in the browser process.
extern const size_t kMaxTitleChars;

extern const bool kRecordModeEnabled;

// Most sequence numbers are used by a renderer when responding to a browser
// request for histogram data.  This reserved number is used when a renderer
// sends an unprovoked update, such as after a page has been loaded.  Using
// this reserved constant avoids any chance of confusion with a response having
// a browser-supplied sequence number.
extern const int kHistogramSynchronizerReservedSequenceNumber;

// The maximum number of session history entries per tab.
extern const int kMaxSessionHistoryEntries;

// The language code used when the language of a page could not be detected.
// (Matches what the CLD -Compact Language Detection- library reports.)
extern const char* const kUnknownLanguageCode;

// If another javascript message box is displayed within
// kJavascriptMessageExpectedDelay of a previous javascript message box being
// dismissed, display an option to suppress future message boxes from this
// contents.
extern const int kJavascriptMessageExpectedDelay;

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
