// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "ui/gfx/canvas_skia.h"
#include "views/controls/menu/menu_model_adapter.h"
#include "views/widget/widget.h"

// Menu should display below the image on the frame. This
// offset size depends on whether the frame is in glass or opaque mode.
const int kMenuDisplayOffset = 5;

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

AvatarMenuButton::AvatarMenuButton(const std::wstring& text,
                                   ui::MenuModel* menu_model)
    : MenuButton(NULL, text, this, false),
      menu_model_(menu_model) {
  // In RTL mode, the avatar icon should be looking the opposite direction.
  EnableCanvasFlippingForRTLUI(true);
}

AvatarMenuButton::~AvatarMenuButton() {}

void AvatarMenuButton::OnPaint(gfx::Canvas* canvas) {
  const SkBitmap& icon = GetImageToPaint();
  if (icon.isNull())
    return;

  // Scale the image to fit the width of the button.
  int dst_width = std::min(icon.width(), width());
  // Truncate rather than rounding, so that for odd widths we put the extra
  // pixel on the left.
  int dst_x = (width() - dst_width) / 2;

  // Scale the height and maintain aspect ratio. This means that the
  // icon may not fit in the view. That's ok, we just vertically center it.
  float scale =
      static_cast<float>(dst_width) / static_cast<float>(icon.width());
  // Round here so that we minimize the aspect ratio drift.
  int dst_height = Round(icon.height() * scale);
  // Round rather than truncating, so that for odd heights we select an extra
  // pixel below the image center rather than above.  This is because the
  // incognito image has shadows at the top that make the apparent center below
  // the real center.
  int dst_y = Round((height() - dst_height) / 2.0);

  canvas->DrawBitmapInt(icon, 0, 0, icon.width(), icon.height(),
      dst_x, dst_y, dst_width, dst_height, false);
}

// views::ViewMenuDelegate implementation
void AvatarMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  if (!menu_model_.get())
    return;

  views::MenuModelAdapter menu_model_adapter(menu_model_.get());
  views::MenuItemView menu(&menu_model_adapter);
  menu_model_adapter.BuildMenu(&menu);

  gfx::Point menu_point(pt.x(), pt.y() + kMenuDisplayOffset);
  menu.RunMenuAt(source->GetWidget()->GetNativeWindow(), NULL,
                 gfx::Rect(pt, gfx::Size(0, 0)),
                 views::MenuItemView::TOPRIGHT,
                 true);
}
