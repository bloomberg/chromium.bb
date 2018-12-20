// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcut_button.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/view_properties.h"

namespace ash {

TopShortcutButton::TopShortcutButton(views::ButtonListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id)
    : TopShortcutButton(listener, accessible_name_id) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, kTrayTopShortcutButtonIconSize,
                                 kUnifiedMenuIconColor));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(icon, kTrayTopShortcutButtonIconSize,
                                 kUnifiedMenuIconColorDisabled));
}

TopShortcutButton::TopShortcutButton(views::ButtonListener* listener,
                                     int accessible_name_id)
    : views::ImageButton(listener) {
  const gfx::Size size(kTrayItemSize, kTrayItemSize);
  SetPreferredSize(size);
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));

  TrayPopupUtils::ConfigureTrayPopupButton(this);
  set_ink_drop_base_color(kUnifiedMenuIconColor);

  auto path = std::make_unique<SkPath>();
  path->addOval(gfx::RectToSkRect(gfx::Rect(size)));
  SetProperty(views::kHighlightPathKey, path.release());
}

TopShortcutButton::~TopShortcutButton() = default;

void TopShortcutButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kUnifiedMenuButtonColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(*GetProperty(views::kHighlightPathKey), flags);

  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> TopShortcutButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

}  // namespace ash
