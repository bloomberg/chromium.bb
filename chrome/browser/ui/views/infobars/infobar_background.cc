// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_background.h"

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

InfoBarBackground::InfoBarBackground() {
}

InfoBarBackground::~InfoBarBackground() {
}

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  InfoBarView* infobar = static_cast<InfoBarView*>(view);
  gfx::ScopedCanvas scoped(canvas);
  // Undo the scale factor so we can stroke with a width of 1px (not 1dp).
  const float dsf = canvas->UndoDeviceScaleFactor();
  // Always scale to a whole number to make sure strokes are sharp.
  // For some fractional DSFs, view size rounding issues may cause there to be a
  // gap above the infobar (below the horizontal stroke painted by the previous
  // infobar or the toolbar). We use floor here to "pull" the fill region up a
  // pixel and bridge the gap. Sometimes this will be 1px too high, so we also
  // re-draw the horizontal portions of the top separator in case this floor
  // operation has covered it up. For whole-number DSFs, the floor should have
  // no effect.
  auto scale = [dsf](int dimension) { return std::floor(dimension * dsf); };
  SkPath fill_path;
  fill_path.addRect(0, 0, scale(infobar->width()), scale(infobar->height()));
  cc::PaintFlags fill;
  fill.setStyle(cc::PaintFlags::kFill_Style);
  fill.setColor(get_color());
  canvas->DrawPath(fill_path, fill);

  // Bottom separator.
  cc::PaintFlags stroke;
  stroke.setStyle(cc::PaintFlags::kStroke_Style);
  const int kSeparatorThicknessPx = 1;
  stroke.setStrokeWidth(SkIntToScalar(kSeparatorThicknessPx));
  SkColor separator_color = view->GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR);
  stroke.setColor(separator_color);
  stroke.setAntiAlias(false);
  gfx::SizeF view_size_px = gfx::ScaleSize(gfx::SizeF(view->size()), dsf);
  SkScalar y = SkIntToScalar(view_size_px.height() - kSeparatorThicknessPx) +
               SK_ScalarHalf;
  SkScalar w = SkFloatToScalar(view_size_px.width());
  canvas->sk_canvas()->drawLine(0, y, w, y, stroke);
}
