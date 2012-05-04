// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "base/file_path.h"

#define FPL FILE_PATH_LITERAL

#if defined(OS_MACOSX)
#define CHROMIUM_PRODUCT_STRING "Chromium"
#if defined(GOOGLE_CHROME_BUILD)
#define PRODUCT_STRING "Google Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING "Chromium"
#else
#error Unknown branding
#endif
#endif  // OS_MACOSX

namespace chrome {

const char kChromeVersionEnvVar[] = "CHROME_VERSION";

// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
//
// There are four constants used to locate the executable name and path:
//
//     kBrowserProcessExecutableName
//     kHelperProcessExecutableName
//     kBrowserProcessExecutablePath
//     kHelperProcessExecutablePath
//
// In one condition, our tests will be built using the Chrome branding
// though we want to actually execute a Chromium branded application.
// This happens for the reference build on Mac.  To support that case,
// we also include a Chromium version of each of the four constants and
// in the UITest class we support switching to that version when told to
// do so.

#if defined(OS_WIN)
const FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL("chrome.exe");
const FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome.exe");
const FilePath::CharType kHelperProcessExecutableNameChromium[] =
    FPL("chrome.exe");
const FilePath::CharType kHelperProcessExecutableName[] = FPL("chrome.exe");
#elif defined(OS_MACOSX)
const FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING);
const FilePath::CharType kBrowserProcessExecutableName[] = FPL(PRODUCT_STRING);
const FilePath::CharType kHelperProcessExecutableNameChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING " Helper");
const FilePath::CharType kHelperProcessExecutableName[] =
    FPL(PRODUCT_STRING " Helper");
#elif defined(OS_POSIX)
const FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL("chrome");
const FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome");
// Helper processes end up with a name of "exe" due to execing via
// /proc/self/exe.  See bug 22703.
const FilePath::CharType kHelperProcessExecutableNameChromium[] = FPL("exe");
const FilePath::CharType kHelperProcessExecutableName[] = FPL("exe");
#endif  // OS_*

#if defined(OS_WIN)
const FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL("chrome.exe");
const FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome.exe");
const FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL("chrome.exe");
const FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome.exe");
#elif defined(OS_MACOSX)
const FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING ".app/Contents/MacOS/" CHROMIUM_PRODUCT_STRING);
const FilePath::CharType kBrowserProcessExecutablePath[] =
    FPL(PRODUCT_STRING ".app/Contents/MacOS/" PRODUCT_STRING);
const FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING " Helper.app/Contents/MacOS/"
        CHROMIUM_PRODUCT_STRING " Helper");
const FilePath::CharType kHelperProcessExecutablePath[] =
    FPL(PRODUCT_STRING " Helper.app/Contents/MacOS/" PRODUCT_STRING " Helper");
#elif defined(OS_POSIX)
const FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL("chrome");
const FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome");
const FilePath::CharType kHelperProcessExecutablePathChromium[] = FPL("chrome");
const FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
#endif  // OS_*

#if defined(OS_MACOSX)
const FilePath::CharType kFrameworkName[] =
    FPL(PRODUCT_STRING " Framework.framework");

const char* const kHelperFlavorSuffixes[] = {
  FPL("EH"),  // Executable heap
  FPL("NP"),  // No PIE
  NULL
};
#endif  // OS_MACOSX

const wchar_t kNaClAppName[] = L"nacl64";
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kBrowserAppName[] = L"Chrome";
#else
const wchar_t kBrowserAppName[] = L"Chromium";
#endif

#if defined(OS_WIN)
const wchar_t kStatusTrayWindowClass[] = L"Chrome_StatusTrayWindow";
#endif  // defined(OS_WIN)

const wchar_t kMessageWindowClass[] = L"Chrome_MessageWindow";
const wchar_t kCrashReportLog[] = L"Reported Crashes.txt";
const wchar_t kTestingInterfaceDLL[] = L"testing_interface.dll";
const char    kInitialProfile[] = "Default";
const char    kMultiProfileDirPrefix[] = "Profile ";
const wchar_t kBrowserResourcesDll[] = L"chrome.dll";
const FilePath::CharType kExtensionFileExtension[] = FPL(".crx");
const FilePath::CharType kExtensionKeyFileExtension[] = FPL(".pem");

// filenames
#if defined(OS_ANDROID)
const FilePath::CharType kAndroidCacheFilename[] = FPL("AndroidCache");
#endif
const FilePath::CharType kArchivedHistoryFilename[] = FPL("Archived History");
const FilePath::CharType kCacheDirname[] = FPL("Cache");
const FilePath::CharType kCRLSetFilename[] =
    FPL("Certificate Revocation Lists");
const FilePath::CharType kMediaCacheDirname[] = FPL("Media Cache");
const FilePath::CharType kOffTheRecordMediaCacheDirname[] =
    FPL("Incognito Media Cache");
const FilePath::CharType kThemePackFilename[] = FPL("Cached Theme.pak");
const FilePath::CharType kCookieFilename[] = FPL("Cookies");
const FilePath::CharType kOBCertFilename[] = FPL("Origin Bound Certs");
const FilePath::CharType kExtensionsCookieFilename[] = FPL("Extension Cookies");
const FilePath::CharType kIsolatedAppStateDirname[] = FPL("Isolated Apps");
const FilePath::CharType kFaviconsFilename[] = FPL("Favicons");
const FilePath::CharType kHistoryFilename[] = FPL("History");
const FilePath::CharType kLocalStateFilename[] = FPL("Local State");
const FilePath::CharType kPreferencesFilename[] = FPL("Preferences");
const FilePath::CharType kSafeBrowsingBaseFilename[] = FPL("Safe Browsing");
const FilePath::CharType kSingletonCookieFilename[] = FPL("SingletonCookie");
const FilePath::CharType kSingletonSocketFilename[] = FPL("SingletonSocket");
const FilePath::CharType kSingletonLockFilename[] = FPL("SingletonLock");
const FilePath::CharType kThumbnailsFilename[] = FPL("Thumbnails");
const FilePath::CharType kNewTabThumbnailsFilename[] = FPL("Top Thumbnails");
const FilePath::CharType kTopSitesFilename[] = FPL("Top Sites");
const wchar_t kUserDataDirname[] = L"User Data";
const FilePath::CharType kWebDataFilename[] = FPL("Web Data");
const FilePath::CharType kBookmarksFileName[] = FPL("Bookmarks");
const FilePath::CharType kHistoryBookmarksFileName[] =
    FPL("Bookmarks From History");
const FilePath::CharType kCustomDictionaryFileName[] =
    FPL("Custom Dictionary.txt");
const FilePath::CharType kLoginDataFileName[] = FPL("Login Data");
const FilePath::CharType kJumpListIconDirname[] = FPL("JumpListIcons");
const FilePath::CharType kWebAppDirname[] = FPL("Web Applications");
const FilePath::CharType kServiceStateFileName[] = FPL("Service State");
const FilePath::CharType kReadmeFilename[] = FPL("README");

#if defined(OS_CHROMEOS)
const FilePath::CharType kGDataCacheDirname[] = FPL("GCache");
#endif  // defined(OS_CHROMEOS)

// File name of the Pepper Flash plugin on different platforms.
const FilePath::CharType kPepperFlashPluginFilename[] =
#if defined(OS_MACOSX)
    FPL("PepperFlashPlayer.plugin");
#elif defined(OS_WIN)
    FPL("pepflashplayer.dll");
#else  // OS_LINUX, etc.
    FPL("libpepflashplayer.so");
#endif

// We don't enable record mode in the released product because users could
// potentially be tricked into running a product in record mode without
// knowing it.  Enable in debug builds.  Playback mode is allowed always,
// because it is useful for testing and not hazardous by itself.
#ifndef NDEBUG
const bool kRecordModeEnabled = true;
#else
const bool kRecordModeEnabled = false;
#endif

const int kHistogramSynchronizerReservedSequenceNumber = 0;

const char* const kUnknownLanguageCode = "und";

const int kJavascriptMessageExpectedDelay = 1000;

const bool kEnableTouchIcon = false;

const float kMaxShareOfExtensionProcesses = 0.30f;

#if defined(OS_LINUX)
extern const int kLowestRendererOomScore = 300;
extern const int kHighestRendererOomScore = 1000;
#endif

#if defined(OS_WIN)
// This is used by the PreRead experiment.
const char kPreReadEnvironmentVariable[] = "CHROME_PRE_READ_EXPERIMENT";
const wchar_t kMetroChromeUserDataSubDir[] = L"Metro";
const wchar_t kMetroNavigationAndSearchMessage[] =
    L"CHROME_METRO_NAV_SEARCH_REQUEST";
const wchar_t kMetroGetCurrentTabInfoMessage[] =
    L"CHROME_METRO_GET_CURRENT_TAB_INFO";
#endif

}  // namespace chrome

#undef FPL
