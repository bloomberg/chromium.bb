// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

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

FilePath GetWebAppDataDirectory(const FilePath& profile_path,
                                const std::string& extension_id,
                                const GURL& url) {
  FilePath app_data_dir(profile_path.Append(chrome::kWebAppDirname));

  if (!extension_id.empty()) {
    return app_data_dir.AppendASCII(
        GenerateApplicationNameFromExtensionId(extension_id));
  }

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if defined(OS_WIN)
  FilePath::StringType host_path(UTF8ToUTF16(host));
  FilePath::StringType scheme_port_path(UTF8ToUTF16(scheme_port));
#elif defined(OS_POSIX)
  FilePath::StringType host_path(host);
  FilePath::StringType scheme_port_path(scheme_port);
#endif

  return app_data_dir.Append(host_path).Append(scheme_port_path);
}

FilePath GetWebAppDataDirectory(const FilePath& profile_path,
                                const extensions::Extension& extension) {
  return GetWebAppDataDirectory(
      profile_path, extension.id(), GURL(extension.launch_web_url()));
}

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
    const FilePath& profile_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&CreateShortcutOnFileThread),
                 profile_path,
                 shortcut_info));
}

bool CreateShortcutOnFileThread(
    const FilePath& profile_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath shortcut_data_dir = GetWebAppDataDirectory(
      profile_path, shortcut_info.extension_id, shortcut_info.url);
  return internals::CreatePlatformShortcut(
      shortcut_data_dir, profile_path, shortcut_info);
}

bool IsValidUrl(const GURL& url) {
  static const char* const kValidUrlSchemes[] = {
      chrome::kFileScheme,
      chrome::kFileSystemScheme,
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

#if defined(TOOLKIT_GTK)
std::string GetWMClassFromAppName(std::string app_name) {
  file_util::ReplaceIllegalCharactersInPath(&app_name, '_');
  TrimString(app_name, "_", &app_name);
  return app_name;
}
#endif

}  // namespace web_app
