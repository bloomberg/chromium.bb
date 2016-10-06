// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_menu_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace ash {

SystemMenuButton::SystemMenuButton(views::ButtonListener* listener,
                                   const gfx::VectorIcon& icon,
                                   int accessible_name_id)
    : views::ImageButton(listener) {
  gfx::ImageSkia image = gfx::CreateVectorIcon(icon, kMenuIconColor);
  SetImage(views::Button::STATE_NORMAL, &image);
  const int horizontal_padding = (kMenuButtonSize - image.width()) / 2;
  const int vertical_padding = (kMenuButtonSize - image.height()) / 2;
  SetBorder(
      views::Border::CreateEmptyBorder(vertical_padding, horizontal_padding,
                                       vertical_padding, horizontal_padding));

  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));

  // TODO(tdanderson): Update the focus rect color, border thickness, and
  // location for material design.
  SetFocusForPlatform();
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 1, 1, 1)));

  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(SK_ColorBLACK);
}

SystemMenuButton::~SystemMenuButton() {}

std::unique_ptr<views::InkDropRipple> SystemMenuButton::CreateInkDropRipple()
    const {
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      GetLocalBounds(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
SystemMenuButton::CreateInkDropHighlight() const {
  return nullptr;
}

bool SystemMenuButton::ShouldShowInkDropForFocus() const {
  return false;
}

}  // namespace ash
