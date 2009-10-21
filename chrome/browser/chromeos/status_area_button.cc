// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status_area_button.h"

#include "app/gfx/canvas.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/resource_bundle.h"
#include "grit/theme_resources.h"
#include "views/border.h"
#include "views/view.h"

////////////////////////////////////////////////////////////////////////////////
// StatusAreaButton

StatusAreaButton::StatusAreaButton(views::ViewMenuDelegate* menu_delegate)
    : MenuButton(NULL, std::wstring(), menu_delegate, false) {
  set_border(NULL);
  SetShowHighlighted(true);
}

void StatusAreaButton::Paint(gfx::Canvas* canvas, bool for_drag) {
  int bitmap_id;

  switch(state()) {
    case BS_NORMAL:
      bitmap_id = IDR_STATUSBAR_CONTAINER;
      break;
    case BS_HOT:
      bitmap_id = IDR_STATUSBAR_CONTAINER_HOVER;
      break;
    case BS_PUSHED:
      bitmap_id = IDR_STATUSBAR_CONTAINER_PRESSED;
      break;
    default:
      bitmap_id = IDR_STATUSBAR_CONTAINER;
      NOTREACHED();
  }
  SkBitmap* container =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(bitmap_id);
  canvas->DrawBitmapInt(*container, 0, 0);
  canvas->DrawBitmapInt(icon(), 0, 0);
}
