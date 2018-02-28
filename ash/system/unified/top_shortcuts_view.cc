// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shutdown_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TopShortcutsView::TopShortcutsView(UnifiedSystemTrayController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(kUnifiedTopShortcutSpacing),
      kUnifiedTopShortcutSpacing));

  // Show the buttons in this row as disabled if the user is at the login
  // screen, lock screen, or in a secondary account flow. The exception is
  // |power_button_| which is always shown as enabled.
  const bool can_show_web_ui = TrayPopupUtils::CanOpenWebUISettings();

  lock_button_ = new TopShortcutButton(this, kSystemMenuLockIcon,
                                       IDS_ASH_STATUS_TRAY_LOCK);
  lock_button_->SetEnabled(can_show_web_ui &&
                           Shell::Get()->session_controller()->CanLockScreen());
  AddChildView(lock_button_);

  settings_button_ = new TopShortcutButton(this, kSystemMenuSettingsIcon,
                                           IDS_ASH_STATUS_TRAY_SETTINGS);
  settings_button_->SetEnabled(can_show_web_ui);
  AddChildView(settings_button_);

  bool reboot = Shell::Get()->shutdown_controller()->reboot_on_shutdown();
  power_button_ = new TopShortcutButton(
      this, kSystemMenuPowerIcon,
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN);
  AddChildView(power_button_);
}

TopShortcutsView::~TopShortcutsView() = default;

void TopShortcutsView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == lock_button_)
    controller_->HandleLockAction();
  else if (sender == settings_button_)
    controller_->HandleSettingsAction();
  else if (sender == power_button_)
    controller_->HandlePowerAction();
}

}  // namespace ash
