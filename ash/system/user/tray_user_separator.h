// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_TRAY_USER_SEPARATOR_H_
#define ASH_SYSTEM_USER_TRAY_USER_SEPARATOR_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {

// This tray item is showing an additional separator line between the logged in
// users and the rest of the system tray menu. The separator will only be shown
// when there are at least two users logged in.
class ASH_EXPORT TrayUserSeparator : public SystemTrayItem {
 public:
  explicit TrayUserSeparator(SystemTray* system_tray);
  ~TrayUserSeparator() override {}

  // Returns true if the separator gets shown.
  bool separator_shown() { return separator_shown_; }

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override {}
  void DestroyDefaultView() override;
  void DestroyDetailedView() override {}
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override {}
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override {}

  // True if the separator gets shown.
  bool separator_shown_;

  DISALLOW_COPY_AND_ASSIGN(TrayUserSeparator);
};

}  // namespace ash

#endif  // ASH_SYSTEM_USER_TRAY_USER_SEPARATOR_H_
