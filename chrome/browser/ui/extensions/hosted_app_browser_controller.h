// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

// Class to encapsulate logic to control the browser UI for hosted apps.
class HostedAppBrowserController {
 public:
  // Indicates whether |browser| is a hosted app browser.
  static bool IsForHostedApp(const Browser* browser);

  // Returns whether |browser| uses the experimental hosted app experience.
  static bool IsForExperimentalHostedAppBrowser(const Browser* browser);

  explicit HostedAppBrowserController(Browser* browser);
  ~HostedAppBrowserController();

  // Whether the browser being controlled should be currently showing the
  // location bar.
  bool ShouldShowLocationBar() const;

  // Updates the location bar visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateLocationBarVisibility(bool animate) const;

  // Returns the app icon for the window to use in the task list.
  gfx::ImageSkia GetWindowAppIcon() const;

  // Returns the icon to be displayed in the window title bar.
  gfx::ImageSkia GetWindowIcon() const;

 private:
  Browser* browser_;
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
