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
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

class UserAvatarButton : public views::Button {
 public:
  UserAvatarButton(views::ButtonListener* listener);
  ~UserAvatarButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAvatarButton);
};

UserAvatarButton::UserAvatarButton(views::ButtonListener* listener)
    : Button(listener) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(CreateUserAvatarView(0 /* user_index */));
}

class TopShortcutButtonContainer : public views::View {
 public:
  TopShortcutButtonContainer();
  ~TopShortcutButtonContainer() override;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TopShortcutButtonContainer);
};

// Buttons are equally spaced by the default value, but the gap will be
// narrowed evenly when the parent view is not large enough.
void TopShortcutButtonContainer::Layout() {
  gfx::Rect child_area(GetContentsBounds());

  int total_horizontal_size = 0;
  int num_visible = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int child_horizontal_size = child->GetPreferredSize().width();
    if (child_horizontal_size == 0)
      continue;
    total_horizontal_size += child_horizontal_size;
    num_visible++;
  }
  int spacing = std::max(kUnifiedTopShortcutButtonMinSpacing,
                         std::min(kUnifiedTopShortcutButtonDefaultSpacing,
                                  (child_area.width() - total_horizontal_size) /
                                      (num_visible - 1)));

  int horizontal_position = child_area.x();
  for (int i = 0; i < child_count(); i++) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    gfx::Rect bounds(child_area);
    bounds.set_x(horizontal_position);
    bounds.set_width(child->GetPreferredSize().width());
    child->SetBoundsRect(bounds);
    horizontal_position += child->GetPreferredSize().width() + spacing;
  }
}

gfx::Size TopShortcutButtonContainer::CalculatePreferredSize() const {
  int total_horizontal_size = 0;
  int max_height = 0;
  int num_visible = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int child_horizontal_size = child->GetPreferredSize().width();
    if (child_horizontal_size == 0)
      continue;
    total_horizontal_size += child_horizontal_size;
    num_visible++;
    max_height = std::max(child->GetPreferredSize().height(), max_height);
  }
  int width =
      (num_visible == 0)
          ? 0
          : total_horizontal_size +
                (num_visible - 1) * kUnifiedTopShortcutButtonDefaultSpacing;
  return gfx::Size(width, max_height);
}

TopShortcutButtonContainer::~TopShortcutButtonContainer() = default;

TopShortcutButtonContainer::TopShortcutButtonContainer() = default;

}  // namespace

TopShortcutsView::TopShortcutsView(UnifiedSystemTrayController* controller)
    : controller_(controller), container_(new TopShortcutButtonContainer()) {
  DCHECK(controller_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kUnifiedTopShortcutPadding,
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(views::BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  AddChildView(container_);

  if (Shell::Get()->session_controller()->login_status() !=
      LoginStatus::NOT_LOGGED_IN) {
    user_avatar_button_ = new UserAvatarButton(this);
    container_->AddChildView(user_avatar_button_);
  }

  // Show the buttons in this row as disabled if the user is at the login
  // screen, lock screen, or in a secondary account flow. The exception is
  // |power_button_| which is always shown as enabled.
  const bool can_show_web_ui = TrayPopupUtils::CanOpenWebUISettings();

  sign_out_button_ = new SignOutButton(this);
  container_->AddChildView(sign_out_button_);

  lock_button_ = new TopShortcutButton(this, kSystemMenuLockIcon,
                                       IDS_ASH_STATUS_TRAY_LOCK);
  lock_button_->SetEnabled(can_show_web_ui &&
                           Shell::Get()->session_controller()->CanLockScreen());
  container_->AddChildView(lock_button_);

  settings_button_ = new TopShortcutButton(this, kSystemMenuSettingsIcon,
                                           IDS_ASH_STATUS_TRAY_SETTINGS);
  settings_button_->SetEnabled(can_show_web_ui);
  container_->AddChildView(settings_button_);

  bool reboot = Shell::Get()->shutdown_controller()->reboot_on_shutdown();
  power_button_ = new TopShortcutButton(
      this, kSystemMenuPowerIcon,
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN);
  container_->AddChildView(power_button_);

  // |collapse_button_| should be right-aligned, so we make the buttons
  // container flex occupying all remaining space.
  layout->SetFlexForView(container_, 1);

  collapse_button_ = new CollapseButton(this);
  AddChildView(collapse_button_);
}

TopShortcutsView::~TopShortcutsView() = default;

void TopShortcutsView::SetExpanded(bool expanded) {
  collapse_button_->UpdateIcon(expanded);
}

void TopShortcutsView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == user_avatar_button_)
    controller_->ShowUserChooserWidget();
  else if (sender == sign_out_button_)
    controller_->HandleSignOutAction();
  else if (sender == lock_button_)
    controller_->HandleLockAction();
  else if (sender == settings_button_)
    controller_->HandleSettingsAction();
  else if (sender == power_button_)
    controller_->HandlePowerAction();
  else if (sender == collapse_button_)
    controller_->ToggleExpanded();
}

}  // namespace ash
