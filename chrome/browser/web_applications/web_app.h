// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/extensions/file_handler_info.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class ImageFamily;
}

namespace web_app {

// This encodes the cause of shortcut creation as the correct behavior in each
// case is implementation specific.
enum ShortcutCreationReason {
  SHORTCUT_CREATION_BY_USER,
  SHORTCUT_CREATION_AUTOMATED,
};

typedef base::Callback<void(const ShellIntegration::ShortcutInfo&)>
    ShortcutInfoCallback;

// Extracts shortcut info of the given WebContents.
void GetShortcutInfoForTab(content::WebContents* web_contents,
                           ShellIntegration::ShortcutInfo* info);

// Updates web app shortcut of the WebContents. This function checks and
// updates web app icon and shortcuts if needed. For icon, the check is based
// on MD5 hash of icon image. For shortcuts, it checks the desktop, start menu
// and quick launch (as well as pinned shortcut) for shortcut and only
// updates (recreates) them if they exits.
void UpdateShortcutForTabContents(content::WebContents* web_contents);

ShellIntegration::ShortcutInfo ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile);

// Fetches the icon for |extension| and calls |callback| with shortcut info
// filled out as by UpdateShortcutInfoForApp.
void UpdateShortcutInfoAndIconForApp(
    const extensions::Extension* extension,
    Profile* profile,
    const ShortcutInfoCallback& callback);

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

// Create shortcuts for web application based on given shortcut data.
// |shortcut_info| contains information about the shortcuts to create, and
// |creation_locations| contains information about where to create them.
void CreateShortcutsForShortcutInfo(
    web_app::ShortcutCreationReason reason,
    const ShellIntegration::ShortcutLocations& locations,
    const ShellIntegration::ShortcutInfo& shortcut_info);

// Creates shortcuts for an app.
void CreateShortcuts(
    ShortcutCreationReason reason,
    const ShellIntegration::ShortcutLocations& locations,
    Profile* profile,
    const extensions::Extension* app);

// Delete all shortcuts that have been created for the given profile and
// extension.
void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app);

// Updates shortcuts for web application based on given shortcut data. This
// refreshes existing shortcuts and their icons, but does not create new ones.
// |old_app_title| contains the title of the app prior to this update.
void UpdateAllShortcuts(const base::string16& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app);

// Returns true if given url is a valid web app url.
bool IsValidUrl(const GURL& url);

#if defined(TOOLKIT_VIEWS)
// Extracts icons info from web app data. Take only square shaped icons and
// sort them from smallest to largest.
typedef std::vector<WebApplicationInfo::IconInfo> IconInfoList;
void GetIconsInfo(const WebApplicationInfo& app_info,
                  IconInfoList* icons);
#endif

#if defined(OS_LINUX)
// Windows that correspond to web apps need to have a deterministic (and
// different) WMClass than normal chrome windows so the window manager groups
// them as a separate application.
std::string GetWMClassFromAppName(std::string app_name);
#endif

namespace internals {

#if defined(OS_WIN)
// Returns the Windows user-level shortcut paths that are specified in
// |creation_locations|.
std::vector<base::FilePath> GetShortcutPaths(
    const ShellIntegration::ShortcutLocations& creation_locations);
#endif

// Creates a shortcut. Must be called on the file thread. This is used to
// implement CreateShortcuts() above, and can also be used directly from the
// file thread. |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
bool CreateShortcutsOnFileThread(
    ShortcutCreationReason reason,
    const ShellIntegration::ShortcutLocations& locations,
    const ShellIntegration::ShortcutInfo& shortcut_info);

// Implemented for each platform, does the platform specific parts of creating
// shortcuts. Used internally by CreateShortcutsOnFileThread.
// |shortcut_data_path| is where to store any resources created for the
// shortcut, and is also used as the UserDataDir for platform app shortcuts.
// |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
bool CreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info,
    const ShellIntegration::ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason);

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
    const base::string16& old_app_title,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info);

// Delete all the shortcuts for an entire profile.
// This is executed on the FILE thread.
void DeleteAllShortcutsForProfile(const base::FilePath& profile_path);

// Sanitizes |name| and returns a version of it that is safe to use as an
// on-disk file name .
base::FilePath GetSanitizedFileName(const base::string16& name);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
