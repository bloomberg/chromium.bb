// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "ui/views/linux_ui/status_icon_linux.h"

// Wrapper class for StatusIconLinux that implements the standard StatusIcon
// interface. Also handles callbacks from StatusIconLinux.
class StatusIconLinuxWrapper : public StatusIcon,
                               public views::StatusIconLinux::Delegate,
                               public StatusIconMenuModel::Observer {
 public:
  virtual ~StatusIconLinuxWrapper();

  // StatusIcon overrides:
  virtual void SetImage(const gfx::ImageSkia& image) OVERRIDE;
  virtual void SetToolTip(const base::string16& tool_tip) OVERRIDE;
  virtual void DisplayBalloon(const gfx::ImageSkia& icon,
                              const base::string16& title,
                              const base::string16& contents) OVERRIDE;

  // StatusIconLinux::Delegate overrides:
  virtual void OnClick() OVERRIDE;
  virtual bool HasClickAction() OVERRIDE;

  // StatusIconMenuModel::Observer overrides:
  virtual void OnMenuStateChanged() OVERRIDE;

  static StatusIconLinuxWrapper* CreateWrappedStatusIcon(
      const gfx::ImageSkia& image,
      const base::string16& tool_tip);

 protected:
  // StatusIcon overrides:
  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. If NULL is
  // passed, subclass should destroy the native context menu.
  virtual void UpdatePlatformContextMenu(
      StatusIconMenuModel* model) OVERRIDE;

 private:
  // A status icon wrapper should only be created by calling
  // CreateWrappedStatusIcon().
  explicit StatusIconLinuxWrapper(views::StatusIconLinux* status_icon);

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  scoped_ptr<views::StatusIconLinux> status_icon_;

  StatusIconMenuModel* menu_model_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxWrapper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
