// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace {

// File name of the internal Flash plugin on different platforms.
const FilePath::CharType kInternalFlashPluginFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("Flash Player Plugin for Chrome.plugin");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("gcswf32.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libgcflashplayer.so");
#endif

// File name of the internal PDF plugin on different platforms.
const FilePath::CharType kInternalPDFPluginFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("pdf.dll");
#elif defined(OS_MACOSX)
    FILE_PATH_LITERAL("PDF.plugin");
#else  // Linux and Chrome OS
    FILE_PATH_LITERAL("libpdf.so");
#endif

// File name of the internal NaCl plugin on different platforms.
const FilePath::CharType kInternalNaClPluginFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("ppGoogleNaClPluginChrome.dll");
#elif defined(OS_MACOSX)
    // TODO(noelallen) Please verify this extention name is correct.
    FILE_PATH_LITERAL("ppGoogleNaClPluginChrome.plugin");
#else  // Linux and Chrome OS
    FILE_PATH_LITERAL("libppGoogleNaClPluginChrome.so");
#endif

}  // namespace

namespace chrome {

// Gets the path for internal plugins.
bool GetInternalPluginsDirectory(FilePath* result) {
#if defined(OS_MACOSX)
  // If called from Chrome, get internal plugins from a subdirectory of the
  // framework.
  if (base::mac::AmIBundled()) {
    *result = chrome::GetFrameworkBundlePath();
    DCHECK(!result->empty());
    *result = result->Append("Internet Plug-Ins");
    return true;
  }
  // In tests, just look in the module directory (below).
#endif

  // The rest of the world expects plugins in the module directory.
  return PathService::Get(base::DIR_MODULE, result);
}

bool PathProvider(int key, FilePath* result) {
  // Some keys are just aliases...
  switch (key) {
    case chrome::DIR_APP:
      return PathService::Get(base::DIR_MODULE, result);
    case chrome::DIR_LOGS:
#ifdef NDEBUG
      // Release builds write to the data dir
      return PathService::Get(chrome::DIR_USER_DATA, result);
#else
      // Debug builds write next to the binary (in the build tree)
#if defined(OS_MACOSX)
      if (!PathService::Get(base::DIR_EXE, result))
        return false;
      if (base::mac::AmIBundled()) {
        // If we're called from chrome, dump it beside the app (outside the
        // app bundle), if we're called from a unittest, we'll already
        // outside the bundle so use the exe dir.
        // exe_dir gave us .../Chromium.app/Contents/MacOS/Chromium.
        *result = result->DirName();
        *result = result->DirName();
        *result = result->DirName();
      }
      return true;
#else
      return PathService::Get(base::DIR_EXE, result);
#endif  // defined(OS_MACOSX)
#endif  // NDEBUG
    case chrome::FILE_RESOURCE_MODULE:
      return PathService::Get(base::FILE_MODULE, result);
  }

  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  FilePath cur;
  switch (key) {
    case chrome::DIR_USER_DATA:
      if (!GetDefaultUserDataDirectory(&cur)) {
        NOTREACHED();
        return false;
      }
      create_dir = true;
      break;
    case chrome::DIR_USER_DOCUMENTS:
      if (!GetUserDocumentsDirectory(&cur))
        return false;
      create_dir = true;
      break;
    case chrome::DIR_DEFAULT_DOWNLOADS_SAFE:
#if defined(OS_WIN)
      if (!GetUserDownloadsDirectorySafe(&cur))
        return false;
      break;
#else
      // Fall through for all other platforms.
#endif
    case chrome::DIR_DEFAULT_DOWNLOADS:
      if (!GetUserDownloadsDirectory(&cur))
        return false;
      // Do not create the download directory here, we have done it twice now
      // and annoyed a lot of users.
      break;
    case chrome::DIR_CRASH_DUMPS:
      // The crash reports are always stored relative to the default user data
      // directory.  This avoids the problem of having to re-initialize the
      // exception handler after parsing command line options, which may
      // override the location of the app's profile directory.
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crash Reports"));
      create_dir = true;
      break;
    case chrome::DIR_USER_DESKTOP:
      if (!GetUserDesktop(&cur))
        return false;
      break;
    case chrome::DIR_RESOURCES:
#if defined(OS_MACOSX)
      cur = base::mac::MainAppBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources"));
#endif
      break;
    case chrome::DIR_SHARED_RESOURCES:
      if (!PathService::Get(chrome::DIR_RESOURCES, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("shared"));
      break;
    case chrome::DIR_INSPECTOR:
      if (!PathService::Get(chrome::DIR_RESOURCES, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("inspector"));
      break;
    case chrome::DIR_APP_DICTIONARIES:
#if defined(OS_LINUX) || defined(OS_MACOSX)
      // We can't write into the EXE dir on Linux, so keep dictionaries
      // alongside the safe browsing database in the user data dir.
      // And we don't want to write into the bundle on the Mac, so push
      // it to the user data dir there also.
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
#else
      if (!PathService::Get(base::DIR_EXE, &cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("Dictionaries"));
      create_dir = true;
      break;
    case chrome::DIR_USER_DATA_TEMP:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Temp"));
      break;
    case chrome::DIR_INTERNAL_PLUGINS:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      break;
    case chrome::DIR_MEDIA_LIBS:
#if defined(OS_MACOSX)
      *result = base::mac::MainAppBundlePath();
      *result = result->Append("Libraries");
      return true;
#else
      return PathService::Get(chrome::DIR_APP, result);
#endif
    case chrome::FILE_LOCAL_STATE:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(chrome::kLocalStateFilename);
      break;
    case chrome::FILE_RECORDED_SCRIPT:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("script.log"));
      break;
    case chrome::FILE_FLASH_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kInternalFlashPluginFileName);
      if (!file_util::PathExists(cur))
        return false;
      break;
    case chrome::FILE_PDF_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kInternalPDFPluginFileName);
      break;
    case chrome::FILE_NACL_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kInternalNaClPluginFileName);
      break;
    case chrome::FILE_RESOURCES_PACK:
#if defined(OS_MACOSX)
      if (base::mac::AmIBundled()) {
        cur = base::mac::MainAppBundlePath();
        cur = cur.Append(FILE_PATH_LITERAL("Resources"))
                 .Append(FILE_PATH_LITERAL("resources.pak"));
        break;
      }
      // If we're not bundled on mac, resources.pak should be next to the
      // binary (e.g., for unit tests).
#endif
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources.pak"));
      break;
#if defined(OS_CHROMEOS)
    case chrome::FILE_CHROMEOS_API:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chromeos"));
      cur = cur.Append(FILE_PATH_LITERAL("libcros.so"));
      break;
#endif
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case chrome::DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    case chrome::DIR_TEST_TOOLS:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("tools"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    case chrome::DIR_POLICY_FILES: {
#if defined(GOOGLE_CHROME_BUILD)
      cur = FilePath(FILE_PATH_LITERAL("/etc/opt/chrome/policies"));
#else
      cur = FilePath(FILE_PATH_LITERAL("/etc/chromium/policies"));
#endif
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    }
#endif
#if defined(OS_MACOSX)
    case chrome::DIR_MANAGED_PREFS: {
      if (!GetLocalLibraryDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Managed Preferences"));
      char* login = getlogin();
      if (!login)
        return false;
      cur = cur.AppendASCII(login);
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    }
#endif
#if defined(OS_CHROMEOS)
    case chrome::DIR_USER_EXTERNAL_EXTENSIONS: {
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("External Extensions"));
      break;
    }
#endif
    default:
      return false;
  }

  if (create_dir && !file_util::PathExists(cur) &&
      !file_util::CreateDirectory(cur))
    return false;

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chrome
