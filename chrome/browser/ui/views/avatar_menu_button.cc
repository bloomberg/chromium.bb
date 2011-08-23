// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/gfx/canvas_skia.h"
#include "views/widget/widget.h"

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

AvatarMenuButton::AvatarMenuButton(Browser* browser, bool has_menu)
    : MenuButton(NULL, std::wstring(), this, false),
      browser_(browser),
      has_menu_(has_menu) {
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

bool AvatarMenuButton::HitTest(const gfx::Point& point) const {
  if (!has_menu_)
    return false;
  return views::MenuButton::HitTest(point);
}

// views::ViewMenuDelegate implementation
void AvatarMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  if (!has_menu_)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_->window()->GetNativeHandle());

  gfx::Point origin;
  views::View::ConvertPointToScreen(this, &origin);
  gfx::Rect bounds(0, 0, width(), height());
  bounds.set_origin(origin);

  AvatarMenuBubbleView* bubble_view = new AvatarMenuBubbleView(browser_);
  // Bubble::Show() takes ownership of the view.
  Bubble::Show(browser_view->GetWidget(), bounds, BubbleBorder::TOP_LEFT,
               bubble_view, bubble_view);
}
