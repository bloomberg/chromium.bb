// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/shell_integration.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

class Profile;

namespace web_app {

typedef base::Callback<void(const ShellIntegration::ShortcutInfo&)>
    ShortcutInfoCallback;

ShellIntegration::ShortcutInfo ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* extension, Profile* profile);

// Extracts shortcut info of the given WebContents.
void GetShortcutInfoForTab(content::WebContents* web_contents,
                           ShellIntegration::ShortcutInfo* info);

// Updates web app shortcut of the WebContents. This function checks and
// updates web app icon and shortcuts if needed. For icon, the check is based
// on MD5 hash of icon image. For shortcuts, it checks the desktop, start menu
// and quick launch (as well as pinned shortcut) for shortcut and only
// updates (recreates) them if they exits.
void UpdateShortcutForTabContents(content::WebContents* web_contents);

// Updates the shortcut info for |extension| and |profile|.
void UpdateShortcutInfoForApp(const extensions::Extension& extension,
                              Profile* profile,
                              ShellIntegration::ShortcutInfo* shortcut_info);

// Fetches the icon for |extension| and calls |callback| with shortcut info
// filled out as by UpdateShortcutInfoForApp.
void UpdateShortcutInfoAndIconForApp(
    const extensions::Extension& extension,
    Profile* profile,
    const ShortcutInfoCallback& callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_
