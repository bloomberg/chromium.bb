// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_button.h"

#include "app/resource_bundle.h"
#include "gfx/canvas.h"
#include "gfx/skbitmap_operations.h"
#include "grit/theme_resources.h"
#include "views/border.h"
#include "views/view.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaButton

StatusAreaButton::StatusAreaButton(views::ViewMenuDelegate* menu_delegate)
    : MenuButton(NULL, std::wstring(), menu_delegate, false) {
  set_border(NULL);
  SetShowHighlighted(true);
}

void StatusAreaButton::Paint(gfx::Canvas* canvas, bool for_drag) {
  if (state() == BS_PUSHED) {
    DrawPressed(canvas);
  }
  DrawIcon(canvas);
  PaintFocusBorder(canvas);
}

gfx::Size StatusAreaButton::GetPreferredSize() {
  // icons are 24x24
  static const int kIconWidth = 24;
  static const int kIconHeight = 24;
  gfx::Insets insets = GetInsets();
  gfx::Size prefsize(kIconWidth + insets.width(),
                     kIconHeight + insets.height());
  return prefsize;
}

void StatusAreaButton::DrawIcon(gfx::Canvas* canvas) {
  canvas->DrawBitmapInt(icon(), 0, 0);
}

}  // namespace chromeos
