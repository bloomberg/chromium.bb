// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_

#include "base/files/file_path.h"
#include "chrome/common/extensions/extension_constants.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class CommandLine;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
}

namespace chrome {

struct AppLaunchParams {
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  extension_misc::LaunchContainer container,
                  WindowOpenDisposition disposition);

  // Helper to create AppLaunchParams using ExtensionPrefs::GetLaunchContainer
  // with ExtensionPrefs::LAUNCH_REGULAR to check for a user-configured
  // container.
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  WindowOpenDisposition disposition);

  // Helper to create AppLaunchParams using event flags that allows user to
  // override the user-configured container using modifier keys.
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  int event_flags);

  // The profile to load the application from.
  Profile* profile;

  // The extension to load.
  const extensions::Extension* extension;

  // The container type to launch the application in.
  extension_misc::LaunchContainer container;

  // If container is TAB, this field controls how the tab is opened.
  WindowOpenDisposition disposition;

  // If non-empty, use override_url in place of the application's launch url.
  GURL override_url;

  // If non-NULL, information from the command line may be passed on to the
  // application.
  const CommandLine* command_line;

  // If non-empty, the current directory from which any relative paths on the
  // command line should be expanded from.
  base::FilePath current_directory;
};

// Open the application in a way specified by |params|.
content::WebContents* OpenApplication(const AppLaunchParams& params);

// Open |url| in an app shortcut window. |override_bounds| param is optional.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// application_launch::OpenApplication().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url,
                                            const gfx::Rect& override_bounds);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
