// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

// Class to encapsulate logic to control the browser UI for web apps.
class WebAppBrowserController : public TabStripModelObserver,
                                public content::WebContentsObserver {
 public:
  ~WebAppBrowserController() override;

  // Returns whether |browser| uses the experimental hosted app experience.
  // Convenience wrapper for checking IsForExperimentalWebAppBrowser() on
  // |browser|'s HostedAppBrowserController if it exists.
  static bool IsForExperimentalWebAppBrowser(const Browser* browser);

  // Renders |url|'s origin as Unicode.
  static base::string16 FormatUrlOrigin(const GURL& url);

  // Returns whether the site is secure based on content's security level.
  static bool IsSiteSecure(const content::WebContents* web_contents);

  // Returns true if this controller is for an experimental web app browser.
  bool IsForExperimentalWebAppBrowser() const;

  // Returns whether this controller was created for an installed PWA.
  virtual bool IsHostedApp() const;

  virtual base::Optional<std::string> GetAppId() const = 0;

  // Returns true if the associated Hosted App is for a PWA.
  virtual bool CreatedForInstalledPwa() const;

  // Whether the browser being controlled should be currently showing the
  // toolbar.
  virtual bool ShouldShowToolbar() const = 0;

  // Returns true if the hosted app buttons should be shown in the frame for
  // this BrowserView.
  virtual bool ShouldShowHostedAppButtonContainer() const = 0;

  // Returns the app icon for the window to use in the task list.
  virtual gfx::ImageSkia GetWindowAppIcon() const = 0;

  // Returns the icon to be displayed in the window title bar.
  virtual gfx::ImageSkia GetWindowIcon() const = 0;

  // Returns the color of the title bar.
  virtual base::Optional<SkColor> GetThemeColor() const;

  // Returns the title to be displayed in the window title bar.
  virtual base::string16 GetTitle() const;

  // Gets the short name of the app.
  virtual std::string GetAppShortName() const = 0;

  // Gets the origin of the app start url suitable for display (e.g
  // example.com.au).
  virtual base::string16 GetFormattedUrlOrigin() const = 0;

  // Gets the launch url for the app.
  virtual GURL GetAppLaunchURL() const = 0;

  virtual bool CanUninstall() const;

  virtual void Uninstall();

  // Returns whether the app is installed (uninstallation may complete within
  // the lifetime of HostedAppBrowserController).
  virtual bool IsInstalled() const;

  // Updates the location bar visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateToolbarVisibility(bool animate) const;

  Browser* browser() const { return browser_; }

  // content::WebContentsObserver:
  void DidChangeThemeColor(base::Optional<SkColor> theme_color) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 protected:
  explicit WebAppBrowserController(Browser* browser);
  // Called by OnTabstripModelChanged().
  virtual void OnTabInserted(content::WebContents* contents);
  virtual void OnTabRemoved(content::WebContents* contents);

 private:
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(WebAppBrowserController);
};

#endif  // CHROME_BROWSER_UI_WEB_APP_BROWSER_CONTROLLER_H_
