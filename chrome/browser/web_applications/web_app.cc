// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#if defined(OS_WIN)
#include <shlobj.h>
#endif  // defined(OS_WIN)

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_file.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/environment.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/icon_util.h"
#endif  // defined(OS_WIN)

using content::BrowserThread;
using content::DownloadFile;

namespace {

// Allow tests to disable shortcut creation, to prevent developers desktops
// becoming overrun with shortcuts.
bool disable_shortcut_creation_for_tests = false;

#if defined(OS_WIN)
const FilePath::CharType kIconChecksumFileExt[] = FILE_PATH_LITERAL(".ico.md5");

// Returns true if |ch| is in visible ASCII range and not one of
// "/ \ : * ? " < > | ; ,".
bool IsValidFilePathChar(char16 c) {
  if (c < 32)
    return false;

  switch (c) {
    case '/':
    case '\\':
    case ':':
    case '*':
    case '?':
    case '"':
    case '<':
    case '>':
    case '|':
    case ';':
    case ',':
      return false;
  };

  return true;
}

#endif  // defined(OS_WIN)

// Returns relative directory of given web app url.
FilePath GetWebAppDir(const ShellIntegration::ShortcutInfo& info) {
  if (!info.extension_id.empty()) {
    std::string app_name =
        web_app::GenerateApplicationNameFromExtensionId(info.extension_id);
#if defined(OS_WIN)
    return FilePath(UTF8ToWide(app_name));
#elif defined(OS_POSIX)
    return FilePath(app_name);
#endif
  } else {
    FilePath::StringType host;
    FilePath::StringType scheme_port;

#if defined(OS_WIN)
    host = UTF8ToWide(info.url.host());
    scheme_port = (info.url.has_scheme() ? UTF8ToWide(info.url.scheme())
        : L"http") + FILE_PATH_LITERAL("_") +
        (info.url.has_port() ? UTF8ToWide(info.url.port()) : L"80");
#elif defined(OS_POSIX)
    host = info.url.host();
    scheme_port = info.url.scheme() + FILE_PATH_LITERAL("_") + info.url.port();
#endif

    return FilePath(host).Append(scheme_port);
  }
}

#if defined(TOOLKIT_VIEWS)
// Predicator for sorting images from largest to smallest.
bool IconPrecedes(const WebApplicationInfo::IconInfo& left,
                  const WebApplicationInfo::IconInfo& right) {
  return left.width < right.width;
}
#endif

#if defined(OS_WIN)
// Calculates image checksum using MD5.
void GetImageCheckSum(const SkBitmap& image, base::MD5Digest* digest) {
  DCHECK(digest);

  SkAutoLockPixels image_lock(image);
  MD5Sum(image.getPixels(), image.getSize(), digest);
}

// Saves |image| as an |icon_file| with the checksum.
bool SaveIconWithCheckSum(const FilePath& icon_file, const SkBitmap& image) {
  if (!IconUtil::CreateIconFileFromSkBitmap(image, icon_file))
    return false;

  base::MD5Digest digest;
  GetImageCheckSum(image, &digest);

  FilePath cheksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));
  return file_util::WriteFile(cheksum_file,
                              reinterpret_cast<const char*>(&digest),
                              sizeof(digest)) == sizeof(digest);
}

// Returns true if |icon_file| is missing or different from |image|.
bool ShouldUpdateIcon(const FilePath& icon_file, const SkBitmap& image) {
  FilePath checksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));

  // Returns true if icon_file or checksum file is missing.
  if (!file_util::PathExists(icon_file) ||
      !file_util::PathExists(checksum_file))
    return true;

  base::MD5Digest persisted_image_checksum;
  if (sizeof(persisted_image_checksum) != file_util::ReadFile(checksum_file,
                      reinterpret_cast<char*>(&persisted_image_checksum),
                      sizeof(persisted_image_checksum)))
    return true;

  base::MD5Digest downloaded_image_checksum;
  GetImageCheckSum(image, &downloaded_image_checksum);

  // Update icon if checksums are not equal.
  return memcmp(&persisted_image_checksum, &downloaded_image_checksum,
                sizeof(base::MD5Digest)) != 0;
}

#endif  // defined(OS_WIN)

void CreateShortcutTask(const FilePath& web_app_path,
                        const FilePath& profile_path,
                        const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (disable_shortcut_creation_for_tests)
    return;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::string shortcut_template;
  if (!ShellIntegration::GetDesktopShortcutTemplate(env.get(),
                                                    &shortcut_template)) {
    return;
  }
  ShellIntegration::CreateDesktopShortcut(shortcut_info, shortcut_template);
  return;  // assuming always success.
#elif defined(OS_WIN)
  // Shortcut paths under which to create shortcuts.
  std::vector<FilePath> shortcut_paths;

  // Locations to add to shortcut_paths.
  struct {
    const bool& use_this_location;
    int location_id;
    const wchar_t* sub_dir;
  } locations[] = {
    {
      shortcut_info.create_on_desktop,
      chrome::DIR_USER_DESKTOP,
      NULL
    }, {
      shortcut_info.create_in_applications_menu,
      base::DIR_START_MENU,
      NULL
    }, {
      shortcut_info.create_in_quick_launch_bar,
      // For Win7, create_in_quick_launch_bar means pinning to taskbar. Use
      // base::PATH_START as a flag for this case.
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          base::PATH_START : base::DIR_APP_DATA,
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          NULL : L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  // Populate shortcut_paths.
  for (int i = 0; i < arraysize(locations); ++i) {
    if (locations[i].use_this_location) {
      FilePath path;

      // Skip the Win7 case.
      if (locations[i].location_id == base::PATH_START)
        continue;

      if (!PathService::Get(locations[i].location_id, &path)) {
        return;
      }

      if (locations[i].sub_dir != NULL)
        path = path.Append(locations[i].sub_dir);

      shortcut_paths.push_back(path);
    }
  }

  bool pin_to_taskbar =
      shortcut_info.create_in_quick_launch_bar &&
      (base::win::GetVersion() >= base::win::VERSION_WIN7);

  // For Win7's pinning support, any shortcut could be used. So we only create
  // the shortcut file when there is no shortcut file will be created. That is,
  // user only selects "Pin to taskbar".
  if (pin_to_taskbar && shortcut_paths.empty()) {
    // Creates the shortcut in web_app_path in this case.
    shortcut_paths.push_back(web_app_path);
  }

  if (shortcut_paths.empty()) {
    return;
  }

  // Ensure web_app_path exists.
  if (!file_util::PathExists(web_app_path) &&
      !file_util::CreateDirectory(web_app_path)) {
    return;
  }

  // Generates file name to use with persisted ico and shortcut file.
  FilePath file_name =
      web_app::internals::GetSanitizedFileName(shortcut_info.title);

  // Creates an ico file to use with shortcut.
  FilePath icon_file = web_app_path.Append(file_name).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  if (!web_app::internals::CheckAndSaveIcon(icon_file,
                                            shortcut_info.favicon)) {
    return;
  }

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    return;
  }

  // Working directory.
  FilePath chrome_folder = chrome_exe.DirName();

  CommandLine cmd_line =
     ShellIntegration::CommandLineArgsForLauncher(shortcut_info.url,
                                                  shortcut_info.extension_id);
  // TODO(evan): we rely on the fact that command_line_string() is
  // properly quoted for a Windows command line.  The method on
  // CommandLine should probably be renamed to better reflect that
  // fact.
  string16 wide_switches(cmd_line.GetCommandLineString());

  // Sanitize description
  string16 description = shortcut_info.description;
  if (description.length() >= MAX_PATH)
    description.resize(MAX_PATH - 1);

  // Generates app id from web app url and profile path.
  std::string app_name =
      web_app::GenerateApplicationNameFromInfo(shortcut_info);
  std::wstring app_id = ShellIntegration::GetAppId(
      UTF8ToWide(app_name), profile_path);

  FilePath shortcut_to_pin;

  bool success = true;
  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    FilePath shortcut_file = shortcut_paths[i].Append(file_name).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));

    int unique_number = DownloadFile::GetUniquePathNumber(shortcut_file);
    if (unique_number == -1) {
      success = false;
      continue;
    } else if (unique_number > 0) {
      DownloadFile::AppendNumberToPath(&shortcut_file, unique_number);
    }

    success &= file_util::CreateShortcutLink(chrome_exe.value().c_str(),
        shortcut_file.value().c_str(),
        chrome_folder.value().c_str(),
        wide_switches.c_str(),
        description.c_str(),
        icon_file.value().c_str(),
        0,
        app_id.c_str());

    // Any shortcut would work for the pinning. We use the first one.
    if (success && pin_to_taskbar && shortcut_to_pin.empty())
      shortcut_to_pin = shortcut_file;
  }

  if (success && pin_to_taskbar) {
    if (!shortcut_to_pin.empty()) {
      success &= file_util::TaskbarPinShortcutLink(
          shortcut_to_pin.value().c_str());
    } else {
      success = false;
    }
  }
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace

namespace web_app {

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
static const char* kCrxAppPrefix = "_crx_";

namespace internals {

#if defined(OS_WIN)
// Returns sanitized name that could be used as a file name
FilePath GetSanitizedFileName(const string16& name) {
  string16 file_name;

  for (size_t i = 0; i < name.length(); ++i) {
    char16 c = name[i];
    if (!IsValidFilePathChar(c))
      c = '_';

    file_name += c;
  }

  return FilePath(file_name);
}

// Saves |image| to |icon_file| if the file is outdated and refresh shell's
// icon cache to ensure correct icon is displayed. Returns true if icon_file
// is up to date or successfully updated.
bool CheckAndSaveIcon(const FilePath& icon_file, const SkBitmap& image) {
  if (ShouldUpdateIcon(icon_file, image)) {
    if (SaveIconWithCheckSum(icon_file, image)) {
      // Refresh shell's icon cache. This call is quite disruptive as user would
      // see explorer rebuilding the icon cache. It would be great that we find
      // a better way to achieve this.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                     NULL, NULL);
    } else {
      return false;
    }
  }

  return true;
}
#endif  // OS_WIN

// Returns data directory for given web app url
FilePath GetWebAppDataDirectory(const FilePath& root_dir,
                                const ShellIntegration::ShortcutInfo& info) {
  return root_dir.Append(GetWebAppDir(info));
}

}  // namespace internals

std::string GenerateApplicationNameFromInfo(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  if (!shortcut_info.extension_id.empty()) {
    return web_app::GenerateApplicationNameFromExtensionId(
        shortcut_info.extension_id);
  } else {
    return web_app::GenerateApplicationNameFromURL(
        shortcut_info.url);
  }
}

std::string GenerateApplicationNameFromURL(const GURL& url) {
  std::string t;
  t.append(url.host());
  t.append("_");
  t.append(url.path());
  return t;
}

std::string GenerateApplicationNameFromExtensionId(const std::string& id) {
  std::string t(web_app::kCrxAppPrefix);
  t.append(id);
  return t;
}

std::string GetExtensionIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

void CreateShortcut(
    const FilePath& data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateShortcutTask,
                 web_app::internals::GetWebAppDataDirectory(
                    web_app::GetDataDir(data_dir),
                    shortcut_info),
                 data_dir,
                 shortcut_info));
}

void SetDisableShortcutCreationForTests(bool disable) {
  disable_shortcut_creation_for_tests = disable;
}

bool IsValidUrl(const GURL& url) {
  static const char* const kValidUrlSchemes[] = {
      chrome::kFileScheme,
      chrome::kFtpScheme,
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
      chrome::kExtensionScheme,
  };

  for (size_t i = 0; i < arraysize(kValidUrlSchemes); ++i) {
    if (url.SchemeIs(kValidUrlSchemes[i]))
      return true;
  }

  return false;
}

FilePath GetDataDir(const FilePath& profile_path) {
  return profile_path.Append(chrome::kWebAppDirname);
}

#if defined(TOOLKIT_VIEWS)
void GetIconsInfo(const WebApplicationInfo& app_info,
                  IconInfoList* icons) {
  DCHECK(icons);

  icons->clear();
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    // We only take square shaped icons (i.e. width == height).
    if (app_info.icons[i].width == app_info.icons[i].height) {
      icons->push_back(app_info.icons[i]);
    }
  }

  std::sort(icons->begin(), icons->end(), &IconPrecedes);
}
#endif

#if defined(TOOLKIT_USES_GTK)
std::string GetWMClassFromAppName(std::string app_name) {
  file_util::ReplaceIllegalCharactersInPath(&app_name, '_');
  TrimString(app_name, "_", &app_name);
  return app_name;
}
#endif

}  // namespace web_app
