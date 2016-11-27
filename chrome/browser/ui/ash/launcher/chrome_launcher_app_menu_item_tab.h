// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "content/public/browser/web_contents_observer.h"

namespace content{
class WebContents;
}

// A menu item controller for a running browser tab. It gets created when an
// application/tab list gets created. It's main purpose is to add the
// activation method to the |ChromeLauncherAppMenuItem| class.
class ChromeLauncherAppMenuItemTab
    : public ChromeLauncherAppMenuItem,
      public content::WebContentsObserver {
 public:
  ChromeLauncherAppMenuItemTab(const base::string16 title,
                               const gfx::Image* icon,
                               content::WebContents* content,
                               bool has_leading_separator);
  bool IsActive() const override;
  bool IsEnabled() const override;
  void Execute(int event_flags) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherAppMenuItemTab);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_
