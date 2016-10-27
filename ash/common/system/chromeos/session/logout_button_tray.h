// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_BUTTON_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/common/login_status.h"
#include "ash/common/system/chromeos/session/logout_button_observer.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button.h"

namespace views {
class LabelButton;
}

namespace ash {

// Adds a logout button to the launcher's status area if enabled by the
// kShowLogoutButtonInTray pref.
class ASH_EXPORT LogoutButtonTray : public TrayBackgroundView,
                                    public LogoutButtonObserver {
 public:
  explicit LogoutButtonTray(WmShelf* wm_shelf);
  ~LogoutButtonTray() override;

  // TrayBackgroundView:
  void SetShelfAlignment(ShelfAlignment alignment) override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // LogoutButtonObserver:
  void OnShowLogoutButtonInTrayChanged(bool show) override;
  void OnLogoutDialogDurationChanged(base::TimeDelta duration) override;

  void UpdateAfterLoginStatusChange(LoginStatus login_status);

 private:
  void UpdateVisibility();
  void UpdateButtonTextAndImage(LoginStatus login_status,
                                ShelfAlignment alignment);

  views::LabelButton* button_;
  LoginStatus login_status_;
  bool show_logout_button_in_tray_;
  base::TimeDelta dialog_duration_;

  DISALLOW_COPY_AND_ASSIGN(LogoutButtonTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_BUTTON_TRAY_H_
