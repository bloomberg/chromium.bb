// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
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

// This namespace contains everything related to integrating Chrome apps into
// the OS. E.g. creating and updating shorcuts for apps, setting up file
// associations, etc.
namespace web_app {

// Represents the info required to create a shortcut for an app.
struct ShortcutInfo {
  ShortcutInfo();
  ~ShortcutInfo();

  GURL url;
  // If |extension_id| is non-empty, this is short cut is to an extension-app
  // and the launch url will be detected at start-up. In this case, |url|
  // is still used to generate the app id (windows app id, not chrome app id).
  std::string extension_id;
  bool is_platform_app = false;
  bool from_bookmark = false;
  base::string16 title;
  base::string16 description;
  base::FilePath extension_path;
  gfx::ImageFamily favicon;
  base::FilePath profile_path;
  std::string profile_name;
  std::string version_for_display;

 private:
  // ShortcutInfo must not be copied; generally it is passed around via
  // unique_ptr. Since ImageFamily has a non-thread-safe reference count in
  // its member and is bound to UI thread, destroy ShortcutInfo instance
  // on UI thread.
  DISALLOW_COPY_AND_ASSIGN(ShortcutInfo);
};

// This specifies a folder in the system applications menu (e.g the Start Menu
// on Windows).
//
// These represent the applications menu root, the "Google Chrome" folder and
// the "Chrome Apps" folder respectively.
//
// APP_MENU_LOCATION_HIDDEN specifies a shortcut that is used to register the
// app with the OS (in order to give its windows shelf icons, and correct icons
// and titles), but the app should not show up in menus or search results.
//
// NB: On Linux, these locations may not be used by the window manager (e.g
// Unity and Gnome Shell).
enum ApplicationsMenuLocation {
  APP_MENU_LOCATION_NONE,
  APP_MENU_LOCATION_ROOT,
  APP_MENU_LOCATION_SUBDIR_CHROME_DEPRECATED,  // TODO(bcwhite) remove this
  APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
  APP_MENU_LOCATION_HIDDEN,
};

// Info about which locations to create app shortcuts in.
struct ShortcutLocations {
  ShortcutLocations();

  bool on_desktop;

  ApplicationsMenuLocation applications_menu_location;

  // For Windows, this refers to quick launch bar prior to Win7. In Win7,
  // this means "pin to taskbar". For Mac/Linux, this could be used for
  // Mac dock or the gnome/kde application launcher. However, those are not
  // implemented yet.
  bool in_quick_launch_bar;
};

// This encodes the cause of shortcut creation as the correct behavior in each
// case is implementation specific.
enum ShortcutCreationReason {
  SHORTCUT_CREATION_BY_USER,
  SHORTCUT_CREATION_AUTOMATED,
};

// Called by GetShortcutInfoForApp after fetching the ShortcutInfo.
typedef base::Callback<void(std::unique_ptr<ShortcutInfo>)>
    ShortcutInfoCallback;

#if defined(TOOLKIT_VIEWS)
// Extracts shortcut info of the given WebContents.
std::unique_ptr<ShortcutInfo> GetShortcutInfoForTab(
    content::WebContents* web_contents);
#endif

// Updates web app shortcut of the WebContents. This function checks and
// updates web app icon and shortcuts if needed. For icon, the check is based
// on MD5 hash of icon image. For shortcuts, it checks the desktop, start menu
// and quick launch (as well as pinned shortcut) for shortcut and only
// updates (recreates) them if they exits.
void UpdateShortcutForTabContents(content::WebContents* web_contents);

std::unique_ptr<ShortcutInfo> ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile);

// Populates a ShortcutInfo for the given |extension| in |profile| and passes
// it to |callback| after asynchronously loading all icon representations.
void GetShortcutInfoForApp(const extensions::Extension* extension,
                           Profile* profile,
                           const ShortcutInfoCallback& callback);

// Whether to create a shortcut for this type of extension.
bool ShouldCreateShortcutFor(web_app::ShortcutCreationReason reason,
                             Profile* profile,
                             const extensions::Extension* extension);

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
std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info);

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
// |locations| contains information about where to create them.
void CreateShortcutsWithInfo(ShortcutCreationReason reason,
                             const ShortcutLocations& locations,
                             std::unique_ptr<ShortcutInfo> shortcut_info);

// Creates shortcuts for an app. This loads the app's icon from disk, and calls
// CreateShortcutsWithInfo(). If you already have a ShortcutInfo with the app's
// icon loaded, you should use CreateShortcutsWithInfo() directly.
void CreateShortcuts(ShortcutCreationReason reason,
                     const ShortcutLocations& locations,
                     Profile* profile,
                     const extensions::Extension* app);

// Delete all shortcuts that have been created for the given profile and
// extension.
void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app);

// Updates shortcuts for |app|, but does not create new ones if shortcuts are
// not present in user-facing locations. Some platforms may still (re)create
// hidden shortcuts to interact correctly with the system shelf.
// |old_app_title| contains the title of the app prior to this update.
// |callback| is invoked once the FILE thread tasks have completed.
void UpdateAllShortcuts(const base::string16& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app,
                        const base::Closure& callback);

// Updates shortcuts for all apps in this profile. This is expected to be called
// on the UI thread.
void UpdateShortcutsForAllApps(Profile* profile,
                               const base::Closure& callback);

// Returns true if given url is a valid web app url.
bool IsValidUrl(const GURL& url);

#if defined(TOOLKIT_VIEWS)
// Extracts icons info from web app data. Take only square shaped icons and
// sort them from smallest to largest.
typedef std::vector<WebApplicationInfo::IconInfo> IconInfoList;
void GetIconsInfo(const WebApplicationInfo& app_info, IconInfoList* icons);
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
    const ShortcutLocations& creation_locations);
#endif

// Implemented for each platform, does the platform specific parts of creating
// shortcuts. Used internally by CreateShortcuts methods.
// |shortcut_data_path| is where to store any resources created for the
// shortcut, and is also used as the UserDataDir for platform app shortcuts.
// |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
bool CreatePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const ShortcutInfo& shortcut_info,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason);

// Delete all the shortcuts we have added for this extension. This is the
// platform specific implementation of the DeleteAllShortcuts function, and
// is executed on the FILE thread.
void DeletePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const ShortcutInfo& shortcut_info);

// Updates all the shortcuts we have added for this extension. This is the
// platform specific implementation of the UpdateAllShortcuts function, and
// is executed on the FILE thread.
void UpdatePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const base::string16& old_app_title,
                             const ShortcutInfo& shortcut_info);

// Delete all the shortcuts for an entire profile.
// This is executed on the FILE thread.
void DeleteAllShortcutsForProfile(const base::FilePath& profile_path);

// Sanitizes |name| and returns a version of it that is safe to use as an
// on-disk file name .
base::FilePath GetSanitizedFileName(const base::string16& name);

// Clears |shortcut_info| and invokes |callback| unless it's null.
void DeleteShortcutInfoOnUIThread(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  const base::Closure& callback);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
