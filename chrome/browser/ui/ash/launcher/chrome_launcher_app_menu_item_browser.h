// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_

#include "ash/public/cpp/shelf_application_menu_item.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;

// A shelf application menu item for a running browser.
class ChromeLauncherAppMenuItemBrowser : public ash::ShelfApplicationMenuItem,
                                         public content::NotificationObserver {
 public:
  ChromeLauncherAppMenuItemBrowser(const base::string16 title,
                                   const gfx::Image* icon,
                                   Browser* browser);
  ~ChromeLauncherAppMenuItemBrowser() override;

  // ash::ShelfApplicationMenuItem:
  void Execute(int event_flags) override;

 private:
  // content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The browser which is associated which this item.
  Browser* browser_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherAppMenuItemBrowser);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_
