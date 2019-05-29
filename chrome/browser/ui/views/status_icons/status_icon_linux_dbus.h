// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_

#include "base/macros.h"
#include "ui/views/linux_ui/status_icon_linux.h"

// A status icon following the StatusNotifierItem specification.
// https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem/
class StatusIconLinuxDbus : public views::StatusIconLinux {
 public:
  StatusIconLinuxDbus(const gfx::ImageSkia& image,
                      const base::string16& tool_tip);
  ~StatusIconLinuxDbus() override;

  // StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxDbus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
