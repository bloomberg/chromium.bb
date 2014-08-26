// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace {

// File name of the internal Flash plugin on different platforms.
const base::FilePath::CharType kInternalFlashPluginFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("Flash Player Plugin for Chrome.plugin");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("gcswf32.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libgcflashplayer.so");
#endif

// The Pepper Flash plugins are in a directory with this name.
const base::FilePath::CharType kPepperFlashBaseDirectory[] =
    FILE_PATH_LITERAL("PepperFlash");

#if defined(OS_WIN)
const base::FilePath::CharType kPepperFlashDebuggerBaseDirectory[] =
    FILE_PATH_LITERAL("Macromed\\Flash");
#endif

// File name of the internal PDF plugin on different platforms.
const base::FilePath::CharType kInternalPDFPluginFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("pdf.dll");
#elif defined(OS_MACOSX)
    FILE_PATH_LITERAL("PDF.plugin");
#else  // Linux and Chrome OS
    FILE_PATH_LITERAL("libpdf.so");
#endif

const base::FilePath::CharType kInternalNaClPluginFileName[] =
    FILE_PATH_LITERAL("internal-nacl-plugin");

const base::FilePath::CharType kEffectsPluginFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("pepper/libppeffects.dll");
#elif defined(OS_MACOSX)
    FILE_PATH_LITERAL("pepper/libppeffects.plugin");
#else  // Linux and Chrome OS
    FILE_PATH_LITERAL("pepper/libppeffects.so");
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)

const base::FilePath::CharType kO1DPluginFileName[] =
    FILE_PATH_LITERAL("pepper/libppo1d.so");

const base::FilePath::CharType kGTalkPluginFileName[] =
    FILE_PATH_LITERAL("pepper/libppgoogletalk.so");

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_LINUX)
// The path to the external extension <id>.json files.
// /usr/share seems like a good choice, see: http://www.pathname.com/fhs/
const base::FilePath::CharType kFilepathSinglePrefExtensions[] =
#if defined(GOOGLE_CHROME_BUILD)
    FILE_PATH_LITERAL("/usr/share/google-chrome/extensions");
#else
    FILE_PATH_LITERAL("/usr/share/chromium/extensions");
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_LINUX)

static base::LazyInstance<base::FilePath>
    g_invalid_specified_user_data_dir = LAZY_INSTANCE_INITIALIZER;

// Gets the path for internal plugins.
bool GetInternalPluginsDirectory(base::FilePath* result) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
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

}  // namespace

namespace chrome {

bool PathProvider(int key, base::FilePath* result) {
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

  base::FilePath cur;
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
    case chrome::DIR_USER_MUSIC:
      if (!GetUserMusicDirectory(&cur))
        return false;
      break;
    case chrome::DIR_USER_PICTURES:
      if (!GetUserPicturesDirectory(&cur))
        return false;
      break;
    case chrome::DIR_USER_VIDEOS:
      if (!GetUserVideosDirectory(&cur))
        return false;
      break;
    case chrome::DIR_DEFAULT_DOWNLOADS_SAFE:
#if defined(OS_WIN) || defined(OS_LINUX)
      if (!GetUserDownloadsDirectorySafe(&cur))
        return false;
      break;
#else
      // Fall through for all other platforms.
#endif
    case chrome::DIR_DEFAULT_DOWNLOADS:
#if defined(OS_ANDROID)
      if (!base::android::GetDownloadsDirectory(&cur))
        return false;
#else
      if (!GetUserDownloadsDirectory(&cur))
        return false;
      // Do not create the download directory here, we have done it twice now
      // and annoyed a lot of users.
#endif
      break;
    case chrome::DIR_CRASH_DUMPS:
#if defined(OS_CHROMEOS)
      // ChromeOS uses a separate directory. See http://crosbug.com/25089
      cur = base::FilePath("/var/log/chrome");
#elif defined(OS_ANDROID)
      if (!base::android::GetCacheDirectory(&cur))
        return false;
#else
      // The crash reports are always stored relative to the default user data
      // directory.  This avoids the problem of having to re-initialize the
      // exception handler after parsing command line options, which may
      // override the location of the app's profile directory.
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("Crash Reports"));
      create_dir = true;
      break;
    case chrome::DIR_RESOURCES:
#if defined(OS_MACOSX)
      cur = base::mac::FrameworkBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources"));
#endif
      break;
    case chrome::DIR_INSPECTOR:
      if (!PathService::Get(chrome::DIR_RESOURCES, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("inspector"));
      break;
    case chrome::DIR_APP_DICTIONARIES:
#if defined(OS_POSIX)
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
    case chrome::DIR_INTERNAL_PLUGINS:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      break;
    case chrome::DIR_PEPPER_FLASH_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kPepperFlashBaseDirectory);
      break;
    case chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(kPepperFlashBaseDirectory);
      break;
    case chrome::DIR_PEPPER_FLASH_DEBUGGER_PLUGIN:
#if defined(OS_WIN)
      if (!PathService::Get(base::DIR_SYSTEM, &cur))
        return false;
      cur = cur.Append(kPepperFlashDebuggerBaseDirectory);
#elif defined(OS_MACOSX)
      // TODO(luken): finalize Mac OS directory paths, current consensus is
      // around /Library/Internet Plug-Ins/PepperFlashPlayer/
      return false;
#else
      return false;
#endif
      break;
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
      break;
    case chrome::FILE_PEPPER_FLASH_PLUGIN:
      if (!PathService::Get(chrome::DIR_PEPPER_FLASH_PLUGIN, &cur))
        return false;
      cur = cur.Append(chrome::kPepperFlashPluginFilename);
      break;
    case chrome::FILE_PDF_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kInternalPDFPluginFileName);
      break;
    case chrome::FILE_EFFECTS_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kEffectsPluginFileName);
      break;
    // TODO(teravest): Remove this case once the internal NaCl plugin is gone.
    // We currently need a path here to look up whether the plugin is disabled
    // and what its permissions are.
    case chrome::FILE_NACL_PLUGIN:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.Append(kInternalNaClPluginFileName);
      break;
    // PNaCl is currenly installable via the component updater or by being
    // simply built-in.  DIR_PNACL_BASE is used as the base directory for
    // installation via component updater.  DIR_PNACL_COMPONENT will be
    // the final location of pnacl, which is a subdir of DIR_PNACL_BASE.
    case chrome::DIR_PNACL_BASE:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("pnacl"));
      break;
    // Where PNaCl files are ultimately located.  The default finds the files
    // inside the InternalPluginsDirectory / build directory, as if it
    // was shipped along with chrome.  The value can be overridden
    // if it is installed via component updater.
    case chrome::DIR_PNACL_COMPONENT:
#if defined(OS_MACOSX)
      // PNaCl really belongs in the InternalPluginsDirectory but actually
      // copying it there would result in the files also being shipped, which
      // we don't want yet. So for now, just find them in the directory where
      // they get built.
      if (!PathService::Get(base::DIR_EXE, &cur))
        return false;
      if (base::mac::AmIBundled()) {
        // If we're called from chrome, it's beside the app (outside the
        // app bundle), if we're called from a unittest, we'll already be
        // outside the bundle so use the exe dir.
        // exe_dir gave us .../Chromium.app/Contents/MacOS/Chromium.
        cur = cur.DirName();
        cur = cur.DirName();
        cur = cur.DirName();
      }
#else
      if (!GetInternalPluginsDirectory(&cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("pnacl"));
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    case chrome::FILE_O1D_PLUGIN:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(kO1DPluginFileName);
      break;
    case chrome::FILE_GTALK_PLUGIN:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(kGTalkPluginFileName);
      break;
#endif
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
#if defined(WIDEVINE_CDM_IS_COMPONENT)
    case chrome::DIR_COMPONENT_WIDEVINE_CDM:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(kWidevineCdmBaseDirectory);
      break;
#endif  // defined(WIDEVINE_CDM_IS_COMPONENT)
    // TODO(xhwang): FILE_WIDEVINE_CDM_ADAPTER has different meanings.
    // In the component case, this is the source adapter. Otherwise, it is the
    // actual Pepper module that gets loaded.
    case chrome::FILE_WIDEVINE_CDM_ADAPTER:
      if (!GetInternalPluginsDirectory(&cur))
        return false;
      cur = cur.AppendASCII(kWidevineCdmAdapterFileName);
      break;
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
    case chrome::FILE_RESOURCES_PACK:
#if defined(OS_MACOSX) && !defined(OS_IOS)
      if (base::mac::AmIBundled()) {
        cur = base::mac::FrameworkBundlePath();
        cur = cur.Append(FILE_PATH_LITERAL("Resources"))
                 .Append(FILE_PATH_LITERAL("resources.pak"));
        break;
      }
#elif defined(OS_ANDROID)
      if (!PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &cur))
        return false;
#else
      // If we're not bundled on mac or Android, resources.pak should be next
      // to the binary (e.g., for unit tests).
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("resources.pak"));
      break;
    case chrome::DIR_RESOURCES_EXTENSION:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources"))
               .Append(FILE_PATH_LITERAL("extension"));
      break;
#if defined(OS_CHROMEOS)
    case chrome::DIR_CHROMEOS_WALLPAPERS:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("wallpapers"));
      break;
    case chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("wallpaper_thumbnails"));
      break;
    case chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("custom_wallpapers"));
      break;
#endif
#if defined(OS_LINUX) && defined(ENABLE_MANAGED_USERS)
    case chrome::DIR_SUPERVISED_USERS_DEFAULT_APPS:
      if (!PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("managed_users"));
      break;
#endif
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case chrome::DIR_GEN_TEST_DATA:
#if defined(OS_ANDROID)
      // On Android, our tests don't have permission to write to DIR_MODULE.
      // gtest/test_runner.py pushes data to external storage.
      if (!PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE, &cur))
        return false;
#else
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("test_data"));
      if (!base::PathExists(cur))  // We don't want to create this.
        return false;
      break;
    case chrome::DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur))  // We don't want to create this.
        return false;
      break;
    case chrome::DIR_TEST_TOOLS:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("tools"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      if (!base::PathExists(cur))  // We don't want to create this
        return false;
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
    case chrome::DIR_POLICY_FILES: {
#if defined(GOOGLE_CHROME_BUILD)
      cur = base::FilePath(FILE_PATH_LITERAL("/etc/opt/chrome/policies"));
#else
      cur = base::FilePath(FILE_PATH_LITERAL("/etc/chromium/policies"));
#endif
      break;
    }
#endif
#if defined(OS_MACOSX) && !defined(OS_IOS)
    case chrome::DIR_MANAGED_PREFS: {
      if (!GetLocalLibraryDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Managed Preferences"));
      char* login = getlogin();
      if (!login)
        return false;
      cur = cur.AppendASCII(login);
      if (!base::PathExists(cur))  // We don't want to create this.
        return false;
      break;
    }
    case chrome::DIR_USER_LIBRARY: {
      if (!GetUserLibraryDirectory(&cur))
        return false;
      if (!base::PathExists(cur))  // We don't want to create this.
        return false;
      break;
    }
    case chrome::DIR_USER_APPLICATIONS: {
      if (!GetUserApplicationsDirectory(&cur))
        return false;
      if (!base::PathExists(cur))  // We don't want to create this.
        return false;
      break;
    }
#endif
#if defined(OS_CHROMEOS) || (defined(OS_MACOSX) && !defined(OS_IOS))
    case chrome::DIR_USER_EXTERNAL_EXTENSIONS: {
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("External Extensions"));
      break;
    }
#endif
#if defined(OS_LINUX)
    case chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS: {
      cur = base::FilePath(kFilepathSinglePrefExtensions);
      break;
    }
#endif
    case chrome::DIR_EXTERNAL_EXTENSIONS:
#if defined(OS_MACOSX) && !defined(OS_IOS)
      if (!chrome::GetGlobalApplicationSupportDirectory(&cur))
        return false;

      cur = cur.Append(FILE_PATH_LITERAL("Google"))
               .Append(FILE_PATH_LITERAL("Chrome"))
               .Append(FILE_PATH_LITERAL("External Extensions"));
      create_dir = false;
#else
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;

      cur = cur.Append(FILE_PATH_LITERAL("extensions"));
      create_dir = true;
#endif
      break;

    case chrome::DIR_DEFAULT_APPS:
#if defined(OS_MACOSX)
      cur = base::mac::FrameworkBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Default Apps"));
#else
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("default_apps"));
#endif
      break;

#if defined(OS_LINUX) || (defined(OS_MACOSX) && !defined(OS_IOS))
    case chrome::DIR_NATIVE_MESSAGING:
#if defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
      cur = base::FilePath(FILE_PATH_LITERAL(
           "/Library/Google/Chrome/NativeMessagingHosts"));
#else
      cur = base::FilePath(FILE_PATH_LITERAL(
          "/Library/Application Support/Chromium/NativeMessagingHosts"));
#endif
#else  // defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
      cur = base::FilePath(FILE_PATH_LITERAL(
          "/etc/opt/chrome/native-messaging-hosts"));
#else
      cur = base::FilePath(FILE_PATH_LITERAL(
          "/etc/chromium/native-messaging-hosts"));
#endif
#endif  // !defined(OS_MACOSX)
      break;

    case chrome::DIR_USER_NATIVE_MESSAGING:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("NativeMessagingHosts"));
      break;
#endif  // defined(OS_LINUX) || (defined(OS_MACOSX) && !defined(OS_IOS))
#if !defined(OS_ANDROID)
    case chrome::DIR_GLOBAL_GCM_STORE:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(kGCMStoreDirname);
      break;
#endif  // !defined(OS_ANDROID)

    default:
      return false;
  }

  // TODO(bauerb): http://crbug.com/259796
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (create_dir && !base::PathExists(cur) &&
      !base::CreateDirectory(cur))
    return false;

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

void SetInvalidSpecifiedUserDataDir(const base::FilePath& user_data_dir) {
  g_invalid_specified_user_data_dir.Get() = user_data_dir;
}

const base::FilePath& GetInvalidSpecifiedUserDataDir() {
  return g_invalid_specified_user_data_dir.Get();
}

}  // namespace chrome
