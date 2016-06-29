// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"

#include <algorithm>

#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

VirtualKeyboardTray::VirtualKeyboardTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf), button_(nullptr) {
  button_ = new views::ImageButton(this);
  if (MaterialDesignController::IsShelfMaterial()) {
    gfx::ImageSkia image_md =
        CreateVectorIcon(gfx::VectorIconId::SHELF_KEYBOARD, kShelfIconColor);
    button_->SetImage(views::CustomButton::STATE_NORMAL, &image_md);
  } else {
    gfx::ImageSkia* image_non_md =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD);
    button_->SetImage(views::CustomButton::STATE_NORMAL, image_non_md);
  }
  button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                             views::ImageButton::ALIGN_MIDDLE);

  tray_container()->AddChildView(button_);
  button_->SetFocusBehavior(FocusBehavior::NEVER);
  SetContentsBackground();
  // The Shell may not exist in some unit tests.
  if (WmShell::HasInstance())
    WmShell::Get()->keyboard_ui()->AddObserver(this);
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // The Shell may not exist in some unit tests.
  if (WmShell::HasInstance())
    WmShell::Get()->keyboard_ui()->RemoveObserver(this);
}

void VirtualKeyboardTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());

  // Pad button size to align with other controls in the system tray.
  const gfx::ImageSkia image =
      button_->GetImage(views::CustomButton::STATE_NORMAL);
  const int size = GetTrayConstant(VIRTUAL_KEYBOARD_BUTTON_SIZE);
  const int vertical_padding = (size - image.height()) / 2;
  int horizontal_padding = (size - image.width()) / 2;
  if (!ash::MaterialDesignController::IsShelfMaterial() &&
      IsHorizontalAlignment(alignment)) {
    // Square up the padding if horizontally aligned. Avoid extra padding when
    // vertically aligned as the button would violate the width constraint on
    // the shelf.
    horizontal_padding += std::max(0, vertical_padding - horizontal_padding);
  }

  button_->SetBorder(views::Border::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
}

base::string16 VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void VirtualKeyboardTray::ClickedOutsideBubble() {}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  WmShell::Get()->keyboard_ui()->Show();
  return true;
}

void VirtualKeyboardTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void VirtualKeyboardTray::OnKeyboardEnabledStateChanged(bool new_value) {
  SetVisible(WmShell::Get()->keyboard_ui()->IsEnabled());
}

}  // namespace ash
