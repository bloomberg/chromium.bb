// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#pragma once

#include "chrome/common/extensions/extension_constants.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class CommandLine;
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace application_launch {

// Open |extension| in |container|, using |disposition| if container type is
// TAB. Returns the WebContents* that was created or NULL. If non-empty,
// |override_url| is used in place of the app launch url. Pass relevant
// information in |command_line| onto platform app as launch data.
// |command_line| can be NULL, indicating there is no launch data to pass on.
// TODO(benwells): Put the parameters to this into an ApplicationLaunchParams
// struct.
content::WebContents* OpenApplication(Profile* profile,
                                      const extensions::Extension* extension,
                                      extension_misc::LaunchContainer container,
                                      const GURL& override_url,
                                      WindowOpenDisposition disposition,
                                      const CommandLine* command_line);

// Opens |url| in a new application panel window for the specified url.
content::WebContents* OpenApplicationPanel(
    Profile* profile,
    const extensions::Extension* extension,
    const GURL& url);

// Opens a new application window for the specified url. If |as_panel| is true,
// the application will be opened as a Browser::Type::APP_PANEL in app panel
// window, otherwise it will be opened as as either Browser::Type::APP a.k.a.
// "thin frame" (if |extension| is NULL) or Browser::Type::EXTENSION_APP (if
// |extension| is non-NULL)./ If |app_browser| is not NULL, it is set to the
// browser that hosts the returned tab.
content::WebContents* OpenApplicationWindow(
    Profile* profile,
    const extensions::Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& url,
    Browser** app_browser);

// Open |url| in an app shortcut window.  If |update_shortcut| is true,
// update the name, description, and favicon of the shortcut.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// Browser::OpenApplicationWindow().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url,
                                            bool update_shortcut);

// Open an application for |extension| using |disposition|.  Returns NULL if
// there are no appropriate existing browser windows for |profile|. If
// non-empty, |override_url| is used in place of the app launch url.
content::WebContents* OpenApplicationTab(Profile* profile,
                                         const extensions::Extension* extension,
                                         const GURL& override_url,
                                         WindowOpenDisposition disposition);

}  // namespace application_launch

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
