// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_

#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

class Browser;
class ChromeLauncherController;
class Profile;

// Item controller for an browser shortcut.
class BrowserShortcutLauncherItemController : public LauncherItemController {
 public:
  BrowserShortcutLauncherItemController(ChromeLauncherController* controller,
                                        Profile* profile);

  virtual ~BrowserShortcutLauncherItemController();

  // LauncherItemController overrides:
  virtual string16 GetTitle() OVERRIDE;
  virtual bool IsCurrentlyShownInWindow(aura::Window* window) const OVERRIDE;
  virtual bool IsOpen() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Launch(int event_flags) OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Clicked(const ui::Event& event) OVERRIDE;
  virtual void OnRemoved() OVERRIDE;
  virtual ChromeLauncherAppMenuItems GetApplicationList(
      int event_flags) OVERRIDE;

 private:
  // Get the favicon for the browser list entry for |web_contents|.
  // Note that for incognito windows the incognito icon will be returned.
  gfx::Image GetBrowserListIcon(content::WebContents* web_contents) const;

  // Get the title for the browser list entry for |web_contents|.
  // If |web_contents| has not loaded, returns "Net Tab".
  string16 GetBrowserListTitle(content::WebContents* web_contents) const;

  // Check if the given |web_contents| is in incognito mode.
  bool IsIncognito(content::WebContents* web_contents) const;

  // Activate a browser - or advance to the next one on the list.
  void ActivateOrAdvanceToNextBrowser();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserShortcutLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
