// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#include <map>

namespace chromeos {
class BrowserView;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

class Browser;
class SkBitmap;
class StatusAreaButton;

class StatusIconChromeOS : public StatusIcon,
                           public content::NotificationObserver {
 public:
  StatusIconChromeOS();
  virtual ~StatusIconChromeOS();

  // Public methods from StatusIcon.
  virtual void SetImage(const SkBitmap& image) OVERRIDE;
  virtual void SetPressedImage(const SkBitmap& image) OVERRIDE;
  virtual void SetToolTip(const string16& tool_tip) OVERRIDE;
  virtual void DisplayBalloon(const SkBitmap& icon,
                              const string16& title,
                              const string16& contents) OVERRIDE;

  // Methods from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Exposed for testing.
  StatusAreaButton* GetButtonForBrowser(Browser* browser);

  views::MenuRunner* menu_runner() const {
    return menu_runner_.get();
  }

 private:
  class NotificationTrayButton;
  typedef std::map<chromeos::BrowserView*, NotificationTrayButton*>
      TrayButtonMap;

  // Protected methods from StatusIcon.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* model) OVERRIDE;

  void AddIconToBrowser(Browser* browser);
  void Clicked();

  // Notification registrar for listening to browser window events.
  content::NotificationRegistrar registrar_;

  // Map of tray icons for each browser window.
  TrayButtonMap tray_button_map_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  // A copy of the last set image. Required to initialize the tray icon
  // in any new browser windows.
  scoped_ptr<SkBitmap> last_image_;

  // The context menu for this icon (if any).
  scoped_ptr<views::MenuModelAdapter> context_menu_adapter_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconChromeOS);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
