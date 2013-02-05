// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/theme_background.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_image_mapper.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

ThemeBackground::ThemeBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {
}

void ThemeBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  gfx::ImageSkia* background;

  if (!browser_view_->IsBrowserTypeNormal()) {
    // Never theme app and popup windows.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (browser_view_->IsActive())
      background = rb.GetImageSkiaNamed(IDR_FRAME);
    else
      background = rb.GetImageSkiaNamed(IDR_THEME_FRAME_INACTIVE);
  } else {
    Profile* profile = browser_view_->browser()->profile();
    ui::ThemeProvider* theme = view->GetThemeProvider();
    DCHECK(theme);  // Should be parented to a Widget when painting.

    if (browser_view_->IsActive()) {
      background = theme->GetImageSkiaNamed(
          profile->IsOffTheRecord() ?
          IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME);
    } else {
      background = theme->GetImageSkiaNamed(
          profile->IsOffTheRecord() ?
          IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE);
    }
  }

  gfx::Point origin;
  views::View::ConvertPointToTarget(view,
                                    browser_view_->frame()->GetFrameView(),
                                    &origin);
  canvas->TileImageInt(*background,
                       origin.x(), origin.y(), 0, 0,
                       view->width(), view->height());
}
