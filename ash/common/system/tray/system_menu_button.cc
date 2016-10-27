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
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace ash {

SystemMenuButton::SystemMenuButton(views::ButtonListener* listener,
                                   InkDropStyle ink_drop_style,
                                   const gfx::VectorIcon& icon,
                                   int accessible_name_id)
    : views::ImageButton(listener), ink_drop_style_(ink_drop_style) {
  gfx::ImageSkia image = gfx::CreateVectorIcon(icon, kMenuIconColor);
  SetImage(views::Button::STATE_NORMAL, &image);
  gfx::ImageSkia disabled_image =
      gfx::CreateVectorIcon(icon, kMenuIconColorDisabled);
  SetImage(views::Button::STATE_DISABLED, &disabled_image);

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

  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
  set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
}

SystemMenuButton::~SystemMenuButton() {}

std::unique_ptr<views::InkDropRipple> SystemMenuButton::CreateInkDropRipple()
    const {
  const gfx::Size size = GetInkDropSize();
  switch (ink_drop_style_) {
    case InkDropStyle::SQUARE:
      return base::MakeUnique<views::SquareInkDropRipple>(
          size, size.width() / 2, size, size.width() / 2,
          GetInkDropCenterBasedOnLastEvent(), GetLocalBounds().CenterPoint(),
          GetInkDropBaseColor(), ink_drop_visible_opacity());
    case InkDropStyle::FLOOD_FILL:
      gfx::Rect bounds = GetLocalBounds();
      bounds.Inset(kTrayPopupInkDropInset, kTrayPopupInkDropInset);
      return base::MakeUnique<views::FloodFillInkDropRipple>(
          bounds, GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
          ink_drop_visible_opacity());
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<views::InkDropHighlight>
SystemMenuButton::CreateInkDropHighlight() const {
  // TODO(bruthig): Show the highlight when the ink drop is active. (See
  // crbug.com/649734)
  if (!ShouldShowInkDropHighlight())
    return nullptr;

  int highlight_radius = 0;
  switch (ink_drop_style_) {
    case InkDropStyle::SQUARE:
      highlight_radius = GetInkDropSize().width() / 2;
      break;
    case InkDropStyle::FLOOD_FILL:
      highlight_radius = 0;
      break;
  }

  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(GetInkDropSize(), highlight_radius,
                                  gfx::RectF(GetLocalBounds()).CenterPoint(),
                                  GetInkDropBaseColor()));
  highlight->set_visible_opacity(kTrayPopupInkDropHighlightOpacity);
  return highlight;
}

bool SystemMenuButton::ShouldShowInkDropHighlight() const {
  return false;
}

gfx::Size SystemMenuButton::GetInkDropSize() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(kTrayPopupInkDropInset, kTrayPopupInkDropInset);
  return bounds.size();
}

}  // namespace ash
