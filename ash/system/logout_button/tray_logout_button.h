// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOGOUT_BUTTON_TRAY_LOGOUT_BUTTON_H_
#define ASH_SYSTEM_LOGOUT_BUTTON_TRAY_LOGOUT_BUTTON_H_

#include "ash/system/logout_button/logout_button_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

namespace tray {
class LogoutButton;
}

// Adds a logout button to the system tray if enabled by the
// kShowLogoutButtonInTray pref.
class TrayLogoutButton : public SystemTrayItem, public LogoutButtonObserver {
 public:
  explicit TrayLogoutButton(SystemTray* system_tray);
  virtual ~TrayLogoutButton();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from LogoutButtonObserver.
  virtual void OnShowLogoutButtonInTrayChanged(bool show) OVERRIDE;

 private:
  tray::LogoutButton* logout_button_;

  DISALLOW_COPY_AND_ASSIGN(TrayLogoutButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_LOGOUT_BUTTON_TRAY_LOGOUT_BUTTON_H_
