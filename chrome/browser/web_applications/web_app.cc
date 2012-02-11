// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Returns relative directory of given web app url.
FilePath GetWebAppDir(const ShellIntegration::ShortcutInfo& info) {
  if (!info.extension_id.empty()) {
    std::string app_name =
        web_app::GenerateApplicationNameFromExtensionId(info.extension_id);
#if defined(OS_WIN)
    return FilePath(UTF8ToUTF16(app_name));
#elif defined(OS_POSIX)
    return FilePath(app_name);
#endif
  } else {
    FilePath::StringType host;
    FilePath::StringType scheme_port;

#if defined(OS_WIN)
    host = UTF8ToUTF16(info.url.host());
    scheme_port = (info.url.has_scheme() ? UTF8ToUTF16(info.url.scheme())
        : L"http") + FILE_PATH_LITERAL("_") +
        (info.url.has_port() ? UTF8ToUTF16(info.url.port()) : L"80");
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

// Returns data directory for given web app url
FilePath GetWebAppDataDirectory(const FilePath& root_dir,
                                const ShellIntegration::ShortcutInfo& info) {
  return root_dir.Append(GetWebAppDir(info));
}

FilePath GetSanitizedFileName(const string16& name) {
#if defined(OS_WIN)
  string16 file_name = name;
#else
  std::string file_name = UTF16ToUTF8(name);
#endif
  file_util::ReplaceIllegalCharactersInPath(&file_name, '_');
  return FilePath(file_name);
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
      base::Bind(&internals::CreateShortcutTask,
                 web_app::internals::GetWebAppDataDirectory(
                      web_app::GetDataDir(data_dir),
                      shortcut_info),
                 data_dir,
                 shortcut_info));
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
