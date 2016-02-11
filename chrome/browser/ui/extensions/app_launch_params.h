// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class Profile;

namespace extensions {
class Extension;
}

struct AppLaunchParams {
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  extensions::LaunchContainer container,
                  WindowOpenDisposition disposition,
                  extensions::AppLaunchSource source);

  // Helper to create AppLaunchParams using event flags that allows user to
  // override the user-configured container using modifier keys, falling back to
  // extensions::GetLaunchContainer() with no modifiers.
  AppLaunchParams(Profile* profile,
                  const extensions::Extension* extension,
                  WindowOpenDisposition disposition,
                  extensions::AppLaunchSource source);

  ~AppLaunchParams();

  // The profile to load the application from.
  Profile* profile;

  // The extension to load.
  std::string extension_id;

  // The container type to launch the application in.
  extensions::LaunchContainer container;

  // If container is TAB, this field controls how the tab is opened.
  WindowOpenDisposition disposition;

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

  // Record where the app is launched from for tracking purpose.
  // Different app may have their own enumeration of sources.
  extensions::AppLaunchSource source;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_
