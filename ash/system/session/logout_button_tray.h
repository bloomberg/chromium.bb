// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_
#define ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/session/logout_button_observer.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
}

namespace ash {
class TrayContainer;
class WmShelf;

// Adds a logout button to the launcher's status area if enabled by the
// kShowLogoutButtonInTray pref.
class ASH_EXPORT LogoutButtonTray : public views::View,
                                    public LogoutButtonObserver,
                                    public views::ButtonListener {
 public:
  explicit LogoutButtonTray(WmShelf* wm_shelf);
  ~LogoutButtonTray() override;

  void UpdateAfterLoginStatusChange();
  void UpdateAfterShelfAlignmentChange();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // LogoutButtonObserver:
  void OnShowLogoutButtonInTrayChanged(bool show) override;
  void OnLogoutDialogDurationChanged(base::TimeDelta duration) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void UpdateVisibility();
  void UpdateButtonTextAndImage();

  WmShelf* const wm_shelf_;
  TrayContainer* const container_;
  views::MdTextButton* const button_;
  bool show_logout_button_in_tray_;
  base::TimeDelta dialog_duration_;

  DISALLOW_COPY_AND_ASSIGN(LogoutButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_
