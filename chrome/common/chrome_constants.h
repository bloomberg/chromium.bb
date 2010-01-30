// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_

#include "base/file_path.h"

namespace chrome {

extern const char kChromeVersion[];

extern const wchar_t kBrowserProcessExecutableName[];
extern const wchar_t kHelperProcessExecutableName[];
extern const wchar_t kBrowserProcessExecutablePath[];
extern const FilePath::CharType kHelperProcessExecutablePath[];
#if defined(OS_MACOSX)
extern const FilePath::CharType kFrameworkName[];
#endif
extern const wchar_t kBrowserAppName[];
#if defined(OS_WIN)
extern const wchar_t kBrowserAppID[];
#endif  // defined(OS_WIN)
extern const wchar_t kMessageWindowClass[];
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const wchar_t kNotSignedInProfile[];
extern const wchar_t kNotSignedInID[];
extern const char    kStatsFilename[];
extern const wchar_t kBrowserResourcesDll[];
extern const wchar_t kNaClAppName[];
extern const FilePath::CharType kExtensionFileExtension[];

// filenames
extern const FilePath::CharType kArchivedHistoryFilename[];
extern const FilePath::CharType kCacheDirname[];
extern const FilePath::CharType kMediaCacheDirname[];
extern const FilePath::CharType kOffTheRecordMediaCacheDirname[];
extern const FilePath::CharType kAppCacheDirname[];
extern const wchar_t kChromePluginDataDirname[];
extern const FilePath::CharType kThemePackFilename[];
extern const FilePath::CharType kCookieFilename[];
extern const FilePath::CharType kExtensionsCookieFilename[];
extern const FilePath::CharType kHistoryFilename[];
extern const FilePath::CharType kLocalStateFilename[];
extern const FilePath::CharType kPreferencesFilename[];
extern const FilePath::CharType kSafeBrowsingFilename[];
extern const FilePath::CharType kSingletonSocketFilename[];
extern const FilePath::CharType kSingletonLockFilename[];
extern const FilePath::CharType kThumbnailsFilename[];
extern const FilePath::CharType kNewTabThumbnailsFilename[];
extern const wchar_t kUserDataDirname[];
extern const FilePath::CharType kUserScriptsDirname[];
extern const FilePath::CharType kWebDataFilename[];
extern const FilePath::CharType kBookmarksFileName[];
extern const FilePath::CharType kPrivacyBlacklistFileName[];
extern const FilePath::CharType kHistoryBookmarksFileName[];
extern const FilePath::CharType kCustomDictionaryFileName[];
extern const FilePath::CharType kLoginDataFileName[];
extern const FilePath::CharType kJumpListIconDirname[];
extern const FilePath::CharType kWebAppDirname[];

extern const unsigned int kMaxRendererProcessCount;
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

// The maximum number of characters of the document's title that we're willing
// to accept in the browser process.
extern const size_t kMaxTitleChars;
// The maximum number of characters in the URL that we're willing to accept
// in the browser process. It is set low enough to avoid damage to the browser
// but high enough that a web site can abuse location.hash for a little storage.
extern const size_t kMaxURLChars;

extern const bool kRecordModeEnabled;

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
