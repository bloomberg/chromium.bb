// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/detachable_toolbar_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/window/non_client_view.h"

// static
void DetachableToolbarView::PaintBackgroundAttachedMode(
    gfx::Canvas* canvas,
    ui::ThemeProvider* theme_provider,
    const gfx::Rect& bounds,
    const gfx::Point& background_origin,
    chrome::HostDesktopType host_desktop_type) {
  canvas->FillRect(bounds,
                   theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));
  canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                       background_origin.x(),
                       background_origin.y(),
                       bounds.x(),
                       bounds.y(),
                       bounds.width(),
                       bounds.height());

  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH) {
    // Ash provides additional lightening at the edges of the toolbar.
    gfx::ImageSkia* toolbar_left =
        theme_provider->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_LEFT);
    canvas->TileImageInt(*toolbar_left,
                         bounds.x(),
                         bounds.y(),
                         toolbar_left->width(),
                         bounds.height());
    gfx::ImageSkia* toolbar_right =
        theme_provider->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_RIGHT);
    canvas->TileImageInt(*toolbar_right,
                         bounds.right() - toolbar_right->width(),
                         bounds.y(),
                         toolbar_right->width(),
                         bounds.height());
  }
}

// static
void DetachableToolbarView::PaintHorizontalBorder(gfx::Canvas* canvas,
                                                  DetachableToolbarView* view,
                                                  bool at_top,
                                                  SkColor color) {
  int thickness = views::NonClientFrameView::kClientEdgeThickness;
  int y = at_top ? 0 : (view->height() - thickness);
  canvas->FillRect(gfx::Rect(0, y, view->width(), thickness), color);
}
