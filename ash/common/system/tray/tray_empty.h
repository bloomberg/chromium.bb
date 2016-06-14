// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_EMPTY_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_EMPTY_H_

#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {

class TrayEmpty : public SystemTrayItem {
 public:
  explicit TrayEmpty(SystemTray* system_tray);
  ~TrayEmpty() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  DISALLOW_COPY_AND_ASSIGN(TrayEmpty);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_EMPTY_H_
