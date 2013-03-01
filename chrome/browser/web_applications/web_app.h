// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/web_apps.h"

namespace extensions {
class Extension;
}

namespace web_app {

// Gets the user data directory for given web app. The path for the directory is
// based on |extension_id|. If |extension_id| is empty then |url| is used
// to construct a unique ID.
base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const std::string& extension_id,
                                      const GURL& url);

// Gets the user data directory to use for |extension| located inside
// |profile_path|.
base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const extensions::Extension& extension);

// Compute a deterministic name based on data in the shortcut_info.
std::string GenerateApplicationNameFromInfo(
    const ShellIntegration::ShortcutInfo& shortcut_info);

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Compute a deterministic name based on an extension/apps's id.
std::string GenerateApplicationNameFromExtensionId(const std::string& id);

// Extracts the extension id from the app name.
std::string GetExtensionIdFromApplicationName(const std::string& app_name);

// Creates shortcuts for web application based on given shortcut data.
// |shortcut_info| contains information about the shortcuts to create, and
// |creation_locations| contains information about where to create them.
void CreateShortcuts(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& creation_locations);

// Delete all the shortcuts that have been created for the given
// |shortcut_data| in the profile with |profile_path|.
void DeleteAllShortcuts(const ShellIntegration::ShortcutInfo& shortcut_info);

// Updates shortcuts for web application based on given shortcut data.
// |shortcut_info| contains information about the shortcuts to update.
void UpdateAllShortcuts(const ShellIntegration::ShortcutInfo& shortcut_info);

// Creates a shortcut. Must be called on the file thread. This is used to
// implement CreateShortcuts() above, and can also be used directly from the
// file thread. |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
bool CreateShortcutsOnFileThread(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& creation_locations);

// Returns true if given url is a valid web app url.
bool IsValidUrl(const GURL& url);

#if defined(TOOLKIT_VIEWS)
// Extracts icons info from web app data. Take only square shaped icons and
// sort them from smallest to largest.
typedef std::vector<WebApplicationInfo::IconInfo> IconInfoList;
void GetIconsInfo(const WebApplicationInfo& app_info,
                  IconInfoList* icons);
#endif

#if defined(TOOLKIT_GTK)
// GTK+ windows that correspond to web apps need to have a deterministic (and
// different) WMClass than normal chrome windows so the window manager groups
// them as a separate application.
std::string GetWMClassFromAppName(std::string app_name);
#endif

namespace internals {

#if defined(OS_WIN)
bool CheckAndSaveIcon(const base::FilePath& icon_file, const SkBitmap& image);
#endif

// Implemented for each platform, does the platform specific parts of creating
// shortcuts. Used internally by CreateShortcutsOnFileThread.
// |shortcut_data_path| is where to store any resources created for the
// shortcut, and is also used as the UserDataDir for platform app shortcuts.
// |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
bool CreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& creation_locations);

// Delete all the shortcuts we have added for this extension. This is the
// platform specific implementation of the DeleteAllShortcuts function, and
// is executed on the FILE thread.
void DeletePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShellIntegration::ShortcutInfo& shortcut_info);

// Updates all the shortcuts we have added for this extension. This is the
// platform specific implementation of the UpdateAllShortcuts function, and
// is executed on the FILE thread.
void UpdatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShellIntegration::ShortcutInfo& shortcut_info);

// Sanitizes |name| and returns a version of it that is safe to use as an
// on-disk file name .
base::FilePath GetSanitizedFileName(const string16& name);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
