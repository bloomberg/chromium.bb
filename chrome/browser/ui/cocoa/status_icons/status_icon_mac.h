// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"

@class MenuController;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon {
 public:
  StatusIconMac();
  virtual ~StatusIconMac();

  // Overridden from StatusIcon.
  virtual void SetImage(const gfx::ImageSkia& image) OVERRIDE;
  virtual void SetPressedImage(const gfx::ImageSkia& image) OVERRIDE;
  virtual void SetToolTip(const string16& tool_tip) OVERRIDE;
  virtual void DisplayBalloon(const gfx::ImageSkia& icon,
                              const string16& title,
                              const string16& contents) OVERRIDE;

  bool HasStatusIconMenu();

 protected:
  // Overridden from StatusIcon.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* model) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, CreateMenu);
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, MenuToolTip);

  void SetToolTip(NSString* toolTip);
  void CreateMenu(ui::MenuModel* model, NSString* toolTip);

  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  scoped_nsobject<NSStatusItem> item_;

  scoped_nsobject<StatusItemController> controller_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  scoped_nsobject<NSString> toolTip_;

  // Status menu shown when right-clicking the system icon, if it has been
  // created by |UpdatePlatformContextMenu|.
  scoped_nsobject<MenuController> menu_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconMac);
};

#endif // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
