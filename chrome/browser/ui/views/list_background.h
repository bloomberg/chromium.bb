// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LIST_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_LIST_BACKGROUND_H_
#pragma once

#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/native_theme_win.h"
#include "views/background.h"

// A background object that paints the scrollable list background,
// which may be rendered by the system visual styles system.
class ListBackground : public views::Background {
 public:
  explicit ListBackground() {
    SkColor list_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
        gfx::NativeTheme::LIST, 1, TS_NORMAL, TMT_FILLCOLOR, COLOR_WINDOW);
    SetNativeControlColor(list_color);
  }
  virtual ~ListBackground() {}

  virtual void Paint(gfx::Canvas* canvas, views::View* view) const {
    HDC dc = canvas->BeginPlatformPaint();
    RECT native_lb = view->GetLocalBounds(true).ToRECT();
    gfx::NativeTheme::instance()->PaintListBackground(dc, true, &native_lb);
    canvas->EndPlatformPaint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ListBackground);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LIST_BACKGROUND_H_

