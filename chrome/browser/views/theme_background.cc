// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/theme_background.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/view.h"

ThemeBackground::ThemeBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {
}

void ThemeBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  int image_name;
  Browser* browser = browser_view_->browser();
  if (browser->window()->IsActive()) {
    image_name = browser->profile()->IsOffTheRecord() ?
        IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
  } else {
    image_name = browser->profile()->IsOffTheRecord() ?
        IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }
  ThemeProvider* theme = browser->profile()->GetThemeProvider();
  SkBitmap* background = theme->GetBitmapNamed(image_name);

  gfx::Point origin(0, 0);
  views::View::ConvertPointToView(view,
                                  browser_view_->frame()->GetFrameView(),
                                  &origin);
#if defined(OS_CHROMEOS)
  const int kCustomFrameBackgroundVerticalOffset = 15;
  // TODO(oshima): Remove this once we fully migrated to views.
  // See http://crbug.com/28580.
  if (browser_view_->IsMaximized()) {
    origin.Offset(0, kCustomFrameBackgroundVerticalOffset + 1);
  }
#endif
  canvas->TileImageInt(*background,
                       origin.x(), origin.y(), 0, 0,
                       view->width(), view->height());
}
