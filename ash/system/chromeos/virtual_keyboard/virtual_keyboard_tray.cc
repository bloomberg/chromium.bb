// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"

#include "ash/keyboard/keyboard_ui.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

VirtualKeyboardTray::VirtualKeyboardTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL) {
  button_ = new views::ImageButton(this);
  button_->SetImage(views::CustomButton::STATE_NORMAL,
                    ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                        IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD));
  button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                             views::ImageButton::ALIGN_MIDDLE);

  tray_container()->AddChildView(button_);
  SetContentsBackground();
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance())
    Shell::GetInstance()->keyboard_ui()->AddObserver(this);
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance())
    Shell::GetInstance()->keyboard_ui()->RemoveObserver(this);
}

void VirtualKeyboardTray::SetShelfAlignment(wm::ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());

  // Pad button size to align with other controls in the system tray.
  const gfx::ImageSkia image = button_->GetImage(
      views::CustomButton::STATE_NORMAL);
  int top_padding = (kTrayBarButtonWidth - image.height()) / 2;
  int left_padding = (kTrayBarButtonWidth - image.width()) / 2;
  int bottom_padding = kTrayBarButtonWidth - image.height() - top_padding;
  int right_padding = kTrayBarButtonWidth - image.width() - left_padding;

  // Square up the padding if horizontally aligned. Avoid extra padding when
  // vertically aligned as the button would violate the width constraint on the
  // shelf.
  if (IsHorizontalAlignment(alignment)) {
    gfx::Insets insets = button_->GetInsets();
    int additional_padding = std::max(0, top_padding - left_padding);
    left_padding += additional_padding;
    right_padding += additional_padding;
  }

  button_->SetBorder(views::Border::CreateEmptyBorder(
      top_padding,
      left_padding,
      bottom_padding,
      right_padding));
}

base::string16 VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
}

bool VirtualKeyboardTray::ClickedOutsideBubble() {
  return false;
}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  Shell::GetInstance()->keyboard_ui()->Show();
  return true;
}

void VirtualKeyboardTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void VirtualKeyboardTray::OnKeyboardEnabledStateChanged(bool new_value) {
  SetVisible(Shell::GetInstance()->keyboard_ui()->IsEnabled());
}

}  // namespace ash
