// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EMPTY_H_
#define ASH_SYSTEM_TRAY_TRAY_EMPTY_H_

#include "ash/system/tray/system_tray_item.h"

namespace ash {

class TrayEmpty : public SystemTrayItem {
 public:
  explicit TrayEmpty(SystemTray* system_tray);
  virtual ~TrayEmpty();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayEmpty);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EMPTY_H_
