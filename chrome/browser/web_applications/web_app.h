// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "webkit/glue/dom_operations.h"

class FilePath;
class Profile;
class TabContents;

namespace web_app {

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Callback after user dismisses CreateShortcutView. "true" indicates
// shortcut is created successfully. Otherwise, it is false.
typedef Callback1<bool>::Type CreateShortcutCallback;

// Creates a shortcut for web application based on given shortcut data.
// |profile_path| is used as root directory for persisted data such as icon.
// Directory layout is similar to what Gears has, i.e. an web application's
// file is stored under "#/host_name/scheme_port", where '#' is the
// |root_dir|.
void CreateShortcut(
    const FilePath& profile_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    CreateShortcutCallback* callback);

// Returns true if given url is a valid web app url.
bool IsValidUrl(const GURL& url);

// Returns data dir for web apps for given profile path.
FilePath GetDataDir(const FilePath& profile_path);

#if defined(TOOLKIT_VIEWS)
// Extracts icons info from web app data. Take only square shaped icons and
// sort them from smallest to largest.
typedef std::vector<webkit_glue::WebApplicationInfo::IconInfo> IconInfoList;
void GetIconsInfo(const webkit_glue::WebApplicationInfo& app_info,
                  IconInfoList* icons);
#endif

// Extracts shortcut info of given TabContents.
void GetShortcutInfoForTab(TabContents* tab_contents,
                           ShellIntegration::ShortcutInfo* info);

// Updates web app shortcut of the TabContents. This function checks and
// updates web app icon and shortcuts if needed. For icon, the check is based
// on MD5 hash of icon image. For shortcuts, it checks the desktop, start menu
// and quick launch (as well as pinned shortcut) for shortcut and only
// updates (recreates) them if they exits.
void UpdateShortcutForTabContents(TabContents* tab_contents);

};  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
