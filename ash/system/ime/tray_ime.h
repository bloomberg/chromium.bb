// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_TRAY_IME_H_
#define ASH_SYSTEM_IME_TRAY_IME_H_

#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray_item.h"

namespace views {
class Label;
}

namespace ash {
struct IMEInfo;

namespace tray {
class IMEDefaultView;
class IMEDetailedView;
class IMENotificationView;
}

class TrayItemView;

class TrayIME : public SystemTrayItem,
                public IMEObserver {
 public:
  explicit TrayIME(SystemTray* system_tray);
  ~TrayIME() override;

 private:
  void UpdateTrayLabel(const IMEInfo& info, size_t count);

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // Overridden from IMEObserver.
  void OnIMERefresh() override;

  TrayItemView* tray_label_;
  tray::IMEDefaultView* default_;
  tray::IMEDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayIME);
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_TRAY_IME_H_
