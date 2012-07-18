// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_

#include "base/file_path.h"
#include "chrome/common/extensions/extension_constants.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class CommandLine;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace application_launch {

struct LaunchParams {
  LaunchParams(Profile* profile,
               const extensions::Extension* extension,
               extension_misc::LaunchContainer container,
               WindowOpenDisposition disposition);

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
  FilePath current_directory;
};

// Open the application in a way specified by |params|.
content::WebContents* OpenApplication(const LaunchParams& params);

// Open |url| in an app shortcut window.  If |update_shortcut| is true,
// update the name, description, and favicon of the shortcut.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// application_launch::OpenApplication().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url);

}  // namespace application_launch

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
