// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_

#include "ash/public/cpp/shelf_application_menu_item.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"

namespace content{
class WebContents;
}

// A shelf application menu item for a running browser tab.
class ChromeLauncherAppMenuItemTab : public ash::ShelfApplicationMenuItem,
                                     public content::WebContentsObserver {
 public:
  ChromeLauncherAppMenuItemTab(const base::string16 title,
                               const gfx::Image* icon,
                               content::WebContents* content);

  // ash::ShelfApplicationMenuItem:
  void Execute(int event_flags) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherAppMenuItemTab);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_TAB_H_
