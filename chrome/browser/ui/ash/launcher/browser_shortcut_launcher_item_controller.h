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

// Item controller for an browser shortcut.
class BrowserShortcutLauncherItemController : public LauncherItemController {
 public:
  explicit BrowserShortcutLauncherItemController(
      ChromeLauncherController* controller);

  virtual ~BrowserShortcutLauncherItemController();

  // Updates the activation state of the Broswer item.
  void UpdateBrowserItemState();

  // Sets the shelf id for the browser window if the browser is represented.
  void SetShelfIDForBrowserWindowContents(Browser* browser,
                                          content::WebContents* web_contents);

  // LauncherItemController overrides:
  virtual bool IsOpen() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Launch(ash::LaunchSource source, int event_flags) OVERRIDE;
  virtual bool Activate(ash::LaunchSource source) OVERRIDE;
  virtual ChromeLauncherAppMenuItems GetApplicationList(
      int event_flags) OVERRIDE;
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root_window) OVERRIDE;
  virtual ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool ShouldShowTooltip() OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  // Get the favicon for the browser list entry for |web_contents|.
  // Note that for incognito windows the incognito icon will be returned.
  gfx::Image GetBrowserListIcon(content::WebContents* web_contents) const;

  // Get the title for the browser list entry for |web_contents|.
  // If |web_contents| has not loaded, returns "Net Tab".
  base::string16 GetBrowserListTitle(content::WebContents* web_contents) const;

  // Check if the given |web_contents| is in incognito mode.
  bool IsIncognito(content::WebContents* web_contents) const;

  // Activate a browser - or advance to the next one on the list.
  void ActivateOrAdvanceToNextBrowser();

  // Returns true when the given |browser| is listed in the browser application
  // list.
  bool IsBrowserRepresentedInBrowserList(Browser* browser);

  DISALLOW_COPY_AND_ASSIGN(BrowserShortcutLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
