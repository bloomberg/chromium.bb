// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_H_

#include "base/string16.h"
#include "ui/gfx/image/image.h"

// A description for a menu item. It contains the menu item description as well
// as the function which gets executed upon menu item click.
class ChromeLauncherAppMenuItem {
 public:
  ChromeLauncherAppMenuItem(const string16 title, const gfx::Image* icon);

  virtual ~ChromeLauncherAppMenuItem();

  // Retrieves the title for this menu option.
  const string16& title() const { return title_; }

  // Retrieves the icon for this menu option.
  const gfx::Image& icon() const { return icon_; }

  // Returns true if the item is active.
  virtual bool IsActive() const;

  // Returns true if item is enabled.
  virtual bool IsEnabled() const;

  // Executes the option.
  virtual void Execute();

 private:
  const string16 title_;
  const gfx::Image icon_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherAppMenuItem);
};
#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_H_
