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
  return static_cast<int>(floor(x + 0.5));
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
  if (!icon.isNull()) {
    // Scale the image to fit the width of the button.
    int src_width = icon.width();
    int src_x = 0;
    int dst_width = std::min(src_width, width());
    int dst_x = Round((width() - dst_width) / 2.0);

    // Scale the height and maintain aspect ratio. This means that the
    // icon may not fit in the view. That's ok, we just center it vertically.
    float scale =
        static_cast<float>(dst_width) / static_cast<float>(icon.width());
    int scaled_height = Round(height() / scale);
    int src_height = std::min(scaled_height, icon.height());
    int src_y = Round((icon.height() - src_height) / 2.0);
    int dst_height = src_height * scale;
    int dst_y = Round((height() - dst_height) / 2.0);

    canvas->DrawBitmapInt(icon, src_x, src_y, src_width, src_height,
        dst_x, dst_y, dst_width, dst_height, false);
  }
}

gfx::Size AvatarMenuButton::GetPreferredAvatarSize() {
  return gfx::Size(38, 31);
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
