// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

struct AppLaunchParams {
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  extensions::LaunchContainer container,
                  WindowOpenDisposition disposition);

  // Helper to create AppLaunchParams using extensions::GetLaunchContainer with
  // LAUNCH_TYPE_REGULAR to check for a user-configured container.
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  WindowOpenDisposition disposition);

  // Helper to create AppLaunchParams using event flags that allows user to
  // override the user-configured container using modifier keys, falling back to
  // extensions::GetLaunchContainer() with no modifiers. |desktop_type|
  // indicates the desktop upon which to launch (Ash or Native).
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  int event_flags,
                  chrome::HostDesktopType desktop_type);

  ~AppLaunchParams();

  // The profile to load the application from.
  Profile* profile;

  // The extension to load.
  std::string extension_id;

  // The container type to launch the application in.
  extensions::LaunchContainer container;

  // If container is TAB, this field controls how the tab is opened.
  WindowOpenDisposition disposition;

  // The desktop type to launch on. Uses GetActiveDesktop() if unspecified.
  chrome::HostDesktopType desktop_type;

  // If non-empty, use override_url in place of the application's launch url.
  GURL override_url;

  // If non-empty, use override_boudns in place of the application's default
  // position and dimensions.
  gfx::Rect override_bounds;

  // If non-empty, information from the command line may be passed on to the
  // application.
  base::CommandLine command_line;

  // If non-empty, the current directory from which any relative paths on the
  // command line should be expanded from.
  base::FilePath current_directory;
};

// Opens the application, possibly prompting the user to re-enable it.
void OpenApplicationWithReenablePrompt(const AppLaunchParams& params);

// Open the application in a way specified by |params|.
content::WebContents* OpenApplication(const AppLaunchParams& params);

// Open |url| in an app shortcut window.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// application_launch::OpenApplication().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url);

// Whether the extension can be launched by sending a
// chrome.app.runtime.onLaunched event.
bool CanLaunchViaEvent(const extensions::Extension* extension);

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
