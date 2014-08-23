// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_

#include "base/values.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;

// A menu item controller for a running browser. It gets created when an
// application list gets created. It's main purpose is to add the activation
// method to the |ChromeLauncherAppMenuItem| class.
class ChromeLauncherAppMenuItemBrowser : public content::NotificationObserver,
                                         public ChromeLauncherAppMenuItem {
 public:
  ChromeLauncherAppMenuItemBrowser(const base::string16 title,
                                   const gfx::Image* icon,
                                   Browser* browser,
                                   bool has_leading_separator);
  virtual ~ChromeLauncherAppMenuItemBrowser();

  virtual bool IsActive() const OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual void Execute(int event_flags) OVERRIDE;

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The browser which is associated which this item.
  Browser* browser_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherAppMenuItemBrowser);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_APP_MENU_ITEM_BROWSER_H_
