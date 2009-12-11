// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "base/file_path.h"

#define FPL FILE_PATH_LITERAL

#if defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
#define PRODUCT_STRING "Google Chrome"
#define PRODUCT_STRING_W L"Google Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING "Chromium"
#define PRODUCT_STRING_W L"Chromium"
#else
#error Unknown branding
#endif
#endif  // OS_MACOSX

namespace chrome {

// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
#if defined(OS_WIN)
const wchar_t kBrowserProcessExecutableName[] = L"chrome.exe";
const wchar_t kHelperProcessExecutableName[] = L"chrome.exe";
#elif defined(OS_LINUX)
const wchar_t kBrowserProcessExecutableName[] = L"chrome";
// Helper processes end up with a name of "exe" due to execing via
// /proc/self/exe.  See bug 22703.
const wchar_t kHelperProcessExecutableName[] = L"exe";
#elif defined(OS_MACOSX)
const wchar_t kBrowserProcessExecutableName[] = PRODUCT_STRING_W;
const wchar_t kHelperProcessExecutableName[] = PRODUCT_STRING_W L" Helper";
#endif  // OS_*
#if defined(OS_WIN)
const wchar_t kBrowserProcessExecutablePath[] = L"chrome.exe";
const FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome.exe");
#elif defined(OS_LINUX)
const wchar_t kBrowserProcessExecutablePath[] = L"chrome";
const FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
#elif defined(OS_MACOSX)
const wchar_t kBrowserProcessExecutablePath[] =
    PRODUCT_STRING_W L".app/Contents/MacOS/" PRODUCT_STRING_W;
const FilePath::CharType kHelperProcessExecutablePath[] =
    FPL(PRODUCT_STRING " Helper.app/Contents/MacOS/" PRODUCT_STRING " Helper");
#endif  // OS_*
#if defined(OS_MACOSX)
const FilePath::CharType kFrameworkName[] =
    FPL(PRODUCT_STRING " Framework.framework");
#endif  // OS_MACOSX
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kBrowserAppName[] = L"Chrome";
const char    kStatsFilename[] = "ChromeStats2";
#else
const wchar_t kBrowserAppName[] = L"Chromium";
const char    kStatsFilename[] = "ChromiumStats2";
#endif

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kBrowserAppID[] = L"Chrome";
#else
const wchar_t kBrowserAppID[] = L"Chromium";
#endif
#endif  // defined(OS_WIN)

const wchar_t kMessageWindowClass[] = L"Chrome_MessageWindow";
const wchar_t kCrashReportLog[] = L"Reported Crashes.txt";
const wchar_t kTestingInterfaceDLL[] = L"testing_interface.dll";
const wchar_t kNotSignedInProfile[] = L"Default";
const wchar_t kNotSignedInID[] = L"not-signed-in";
const wchar_t kBrowserResourcesDll[] = L"chrome.dll";
const FilePath::CharType kExtensionFileExtension[] = FPL("crx");

// filenames
const FilePath::CharType kArchivedHistoryFilename[] = FPL("Archived History");
const FilePath::CharType kCacheDirname[] = FPL("Cache");
const FilePath::CharType kMediaCacheDirname[] = FPL("Media Cache");
const FilePath::CharType kOffTheRecordMediaCacheDirname[] =
    FPL("Incognito Media Cache");
const FilePath::CharType kAppCacheDirname[] = FPL("Application Cache");
const wchar_t kChromePluginDataDirname[] = L"Plugin Data";
const FilePath::CharType kThemeImagesDirname[] = FPL("Cached Theme Images");
const FilePath::CharType kCookieFilename[] = FPL("Cookies");
const FilePath::CharType kExtensionsCookieFilename[] = FPL("Extension Cookies");
const FilePath::CharType kHistoryFilename[] = FPL("History");
const FilePath::CharType kLocalStateFilename[] = FPL("Local State");
const FilePath::CharType kPreferencesFilename[] = FPL("Preferences");
const FilePath::CharType kSafeBrowsingFilename[] = FPL("Safe Browsing");
// WARNING: SingletonSocket can't contain spaces, because otherwise
// chrome_process_util_linux would be broken.
const FilePath::CharType kSingletonSocketFilename[] = FPL("SingletonSocket");
const FilePath::CharType kSingletonLockFilename[] = FPL("SingletonLock");
const FilePath::CharType kThumbnailsFilename[] = FPL("Thumbnails");
const FilePath::CharType kNewTabThumbnailsFilename[] = FPL("Top Thumbnails");
const wchar_t kUserDataDirname[] = L"User Data";
const FilePath::CharType kUserScriptsDirname[] = FPL("User Scripts");
const FilePath::CharType kWebDataFilename[] = FPL("Web Data");
const FilePath::CharType kPrivacyBlacklistFileName[] = FPL("Privacy Blacklist");
const FilePath::CharType kBookmarksFileName[] = FPL("Bookmarks");
const FilePath::CharType kHistoryBookmarksFileName[] =
    FPL("Bookmarks From History");
const FilePath::CharType kCustomDictionaryFileName[] =
    FPL("Custom Dictionary.txt");
const FilePath::CharType kLoginDataFileName[] = FPL("Login Data");
const FilePath::CharType kJumpListIconDirname[] = FPL("JumpListIcons");
const FilePath::CharType kWebAppDirname[] = FPL("Web Applications");

// This number used to be limited to 32 in the past (see b/535234).
const unsigned int kMaxRendererProcessCount = 42;
const int kStatsMaxThreads = 32;
const int kStatsMaxCounters = 300;

const size_t kMaxTitleChars = 4 * 1024;

// We don't enable record mode in the released product because users could
// potentially be tricked into running a product in record mode without
// knowing it.  Enable in debug builds.  Playback mode is allowed always,
// because it is useful for testing and not hazardous by itself.
#ifndef NDEBUG
const bool kRecordModeEnabled = true;
#else
const bool kRecordModeEnabled = false;
#endif

}  // namespace chrome

#undef FPL
