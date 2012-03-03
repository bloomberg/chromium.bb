// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#include <vector>

namespace ash {

class SystemTrayItem;

class ASH_EXPORT SystemTray : public views::View,
                              public views::Widget::Observer {
 public:
  SystemTray();
  virtual ~SystemTray();

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  // Shows details of a particular item.
  void ShowDetailedView(SystemTrayItem* item);

  // Updates the items when the login status of the system changes.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  const std::vector<SystemTrayItem*>& items() const { return items_; }

 private:
  void ShowItems(std::vector<SystemTrayItem*>& items, bool details);

  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  std::vector<SystemTrayItem*> items_;

  // The popup widget.
  views::Widget* popup_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
