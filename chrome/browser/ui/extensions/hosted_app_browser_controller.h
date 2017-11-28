// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

class Extension;

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

  // Returns the color of the title bar.
  base::Optional<SkColor> GetThemeColor() const;

  // Returns the title to be displayed in the window title bar.
  base::string16 GetTitle() const;

  // Gets the short name of the app.
  std::string GetAppShortName() const;

  // Gets the domain and registry of the app start url (e.g example.com.au).
  std::string GetDomainAndRegistry() const;

 private:
  // Gets the extension for this controller.
  const Extension* GetExtension() const;

  Browser* const browser_;
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
