// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_menu_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace ash {

SystemMenuButton::SystemMenuButton(views::ButtonListener* listener,
                                   TrayPopupInkDropStyle ink_drop_style,
                                   gfx::ImageSkia normal_icon,
                                   gfx::ImageSkia disabled_icon,
                                   int accessible_name_id)
    : views::ImageButton(listener), ink_drop_style_(ink_drop_style) {
  DCHECK_EQ(normal_icon.width(), disabled_icon.width());
  DCHECK_EQ(normal_icon.height(), disabled_icon.height());

  SetImage(views::Button::STATE_NORMAL, &normal_icon);
  SetImage(views::Button::STATE_DISABLED, &disabled_icon);

  const int horizontal_padding = (kMenuButtonSize - normal_icon.width()) / 2;
  const int vertical_padding = (kMenuButtonSize - normal_icon.height()) / 2;
  SetBorder(views::CreateEmptyBorder(vertical_padding, horizontal_padding,
                                     vertical_padding, horizontal_padding));

  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));

  // TODO(tdanderson): Update the focus rect color, border thickness, and
  // location for material design.
  SetFocusForPlatform();
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 1, 1, 1)));

  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
  set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
}

SystemMenuButton::SystemMenuButton(views::ButtonListener* listener,
                                   TrayPopupInkDropStyle ink_drop_style,
                                   const gfx::VectorIcon& icon,
                                   int accessible_name_id)
    : SystemMenuButton(listener,
                       ink_drop_style,
                       gfx::CreateVectorIcon(icon, kMenuIconColor),
                       gfx::CreateVectorIcon(icon, kMenuIconColorDisabled),
                       accessible_name_id) {}

SystemMenuButton::~SystemMenuButton() {}

void SystemMenuButton::SetInkDropColor(SkColor color) {
  ink_drop_color_ = color;
}

std::unique_ptr<views::InkDrop> SystemMenuButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(ink_drop_style_, this);
}

std::unique_ptr<views::InkDropRipple> SystemMenuButton::CreateInkDropRipple()
    const {
  return ink_drop_color_ == base::nullopt
             ? TrayPopupUtils::CreateInkDropRipple(
                   ink_drop_style_, this, GetInkDropCenterBasedOnLastEvent())
             : TrayPopupUtils::CreateInkDropRipple(
                   ink_drop_style_, this, GetInkDropCenterBasedOnLastEvent(),
                   ink_drop_color_.value());
}

std::unique_ptr<views::InkDropHighlight>
SystemMenuButton::CreateInkDropHighlight() const {
  return ink_drop_color_ == base::nullopt
             ? TrayPopupUtils::CreateInkDropHighlight(ink_drop_style_, this)
             : TrayPopupUtils::CreateInkDropHighlight(ink_drop_style_, this,
                                                      ink_drop_color_.value());
}

std::unique_ptr<views::InkDropMask> SystemMenuButton::CreateInkDropMask()
    const {
  return TrayPopupUtils::CreateInkDropMask(ink_drop_style_, this);
}

}  // namespace ash
