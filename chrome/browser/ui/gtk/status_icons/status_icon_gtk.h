// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_
#define CHROME_BROWSER_UI_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "ui/base/gtk/gtk_signal.h"

class MenuGtk;
class SkBitmap;

class StatusIconGtk : public StatusIcon {
 public:
  StatusIconGtk();
  virtual ~StatusIconGtk();

  // Overridden from StatusIcon:
  virtual void SetImage(const SkBitmap& image) OVERRIDE;
  virtual void SetPressedImage(const SkBitmap& image) OVERRIDE;
  virtual void SetToolTip(const string16& tool_tip) OVERRIDE;
  virtual void DisplayBalloon(const SkBitmap& icon,
                              const string16& title,
                              const string16& contents) OVERRIDE;

  // Exposed for testing.
  CHROMEGTK_CALLBACK_0(StatusIconGtk, void, OnClick);

 protected:
  // Overridden from StatusIcon.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* menu) OVERRIDE;

 private:
  // Callback invoked when user right-clicks on the status icon.
  CHROMEGTK_CALLBACK_2(StatusIconGtk, void, OnPopupMenu, guint, guint);

  // The currently-displayed icon for the window.
  GtkStatusIcon* icon_;

  // The context menu for this icon (if any).
  scoped_ptr<MenuGtk> menu_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_
