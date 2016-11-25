// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"

#include <algorithm>

#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/controls/image_view.h"

namespace ash {

VirtualKeyboardTray::VirtualKeyboardTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      icon_(new views::ImageView),
      wm_shelf_(wm_shelf) {
  if (MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetContentsBackground(false);
    gfx::ImageSkia image_md =
        CreateVectorIcon(gfx::VectorIconId::SHELF_KEYBOARD, kShelfIconColor);
    icon_->SetImage(image_md);
  } else {
    SetContentsBackground(true);
    gfx::ImageSkia* image_non_md =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD);
    icon_->SetImage(image_non_md);
  }

  SetIconBorderForShelfAlignment();
  tray_container()->AddChildView(icon_);
  // The Shell may not exist in some unit tests.
  if (WmShell::HasInstance())
    WmShell::Get()->keyboard_ui()->AddObserver(this);
  // Try observing keyboard controller, in case it is already constructed.
  ObserveKeyboardController();
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // Try unobserving keyboard controller, in case it still exists.
  UnobserveKeyboardController();
  // The Shell may not exist in some unit tests.
  if (WmShell::HasInstance())
    WmShell::Get()->keyboard_ui()->RemoveObserver(this);
}

void VirtualKeyboardTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;

  TrayBackgroundView::SetShelfAlignment(alignment);
  SetIconBorderForShelfAlignment();
}

base::string16 VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void VirtualKeyboardTray::ClickedOutsideBubble() {}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  const int64_t display_id =
      wm_shelf_->GetWindow()->GetDisplayNearestWindow().id();
  WmShell::Get()->keyboard_ui()->ShowInDisplay(display_id);
  // Normally, active status is set when virtual keyboard is shown/hidden,
  // however, showing virtual keyboard happens asynchronously and, especially
  // the first time, takes some time. We need to set active status here to
  // prevent bad things happening if user clicked the button before keyboard is
  // shown.
  SetIsActive(true);
  return true;
}

void VirtualKeyboardTray::OnKeyboardEnabledStateChanged(bool new_enabled) {
  SetVisible(new_enabled);
  if (new_enabled) {
    // Observe keyboard controller to detect when the virtual keyboard is
    // shown/hidden.
    ObserveKeyboardController();
  } else {
    // Try unobserving keyboard controller, in case it is not yet destroyed.
    UnobserveKeyboardController();
  }
}

void VirtualKeyboardTray::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  SetIsActive(!new_bounds.IsEmpty());
}

void VirtualKeyboardTray::OnKeyboardClosed() {}

void VirtualKeyboardTray::SetIconBorderForShelfAlignment() {
  // Every time shelf alignment is updated, StatusAreaWidgetDelegate resets the
  // border to a non-null border. So, we need to remove it.
  if (!ash::MaterialDesignController::IsShelfMaterial())
    tray_container()->SetBorder(views::NullBorder());
  const gfx::ImageSkia& image = icon_->GetImage();
  const int size = GetTrayConstant(VIRTUAL_KEYBOARD_BUTTON_SIZE);
  const int vertical_padding = (size - image.height()) / 2;
  int horizontal_padding = (size - image.width()) / 2;
  if (!ash::MaterialDesignController::IsShelfMaterial() &&
      IsHorizontalAlignment(shelf_alignment())) {
    // Square up the padding if horizontally aligned. Avoid extra padding when
    // vertically aligned as the button would violate the width constraint on
    // the shelf.
    horizontal_padding += std::max(0, vertical_padding - horizontal_padding);
  }
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
}

void VirtualKeyboardTray::ObserveKeyboardController() {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->AddObserver(this);
}

void VirtualKeyboardTray::UnobserveKeyboardController() {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);
}

}  // namespace ash
