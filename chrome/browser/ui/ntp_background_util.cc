// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ntp_background_util.h"

#include <cmath>

#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace {

void PaintThemeBackground(
    gfx::Canvas* canvas, gfx::ImageSkia* ntp_background, int tiling,
    int alignment, const gfx::Rect& area, int tab_contents_height) {
  int x_pos = 0;
  int y_pos = 0;
  int width = area.width() + ntp_background->width();
  int height = area.height() + ntp_background->height();

  if (alignment & ThemeProperties::ALIGN_BOTTOM) {
    y_pos += area.height() + tab_contents_height - ntp_background->height();
  } else if (alignment & ThemeProperties::ALIGN_TOP) {
    // no op
  } else {  // ALIGN_CENTER
    y_pos += std::floor(area.height() + tab_contents_height / 2.0 -
        ntp_background->height() / 2.0 + 0.5);
  }

  if (alignment & ThemeProperties::ALIGN_RIGHT) {
    x_pos += area.width() - ntp_background->width();
  } else if (alignment & ThemeProperties::ALIGN_LEFT) {
    // no op
  } else {  // ALIGN_CENTER
    x_pos +=
        std::floor(area.width() / 2.0 - ntp_background->width() / 2.0 + 0.5);
  }

  if (tiling != ThemeProperties::REPEAT &&
      tiling != ThemeProperties::REPEAT_X) {
    width = ntp_background->width();
  } else if (x_pos > 0) {
    x_pos = x_pos % ntp_background->width() - ntp_background->width();
  }

  if (tiling != ThemeProperties::REPEAT &&
      tiling != ThemeProperties::REPEAT_Y) {
    height = ntp_background->height();
  } else if (y_pos > 0) {
    y_pos = y_pos % ntp_background->height() - ntp_background->height();
  }

  x_pos += area.x();
  y_pos += area.y();

  canvas->TileImageInt(*ntp_background, x_pos, y_pos, width, height);
}

}  // namespace

// static
void NtpBackgroundUtil::PaintBackgroundDetachedMode(ui::ThemeProvider* tp,
                                                    gfx::Canvas* canvas,
                                                    const gfx::Rect& area,
                                                    int tab_contents_height) {
  // Draw the background to match the new tab page.
  canvas->FillRect(area, tp->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND));

  if (tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    int tiling = ThemeProperties::NO_REPEAT;
    tp->GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING, &tiling);
    int alignment;
    if (tp->GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT,
                               &alignment)) {
      gfx::ImageSkia* ntp_background =
          tp->GetImageSkiaNamed(IDR_THEME_NTP_BACKGROUND);

      PaintThemeBackground(
          canvas, ntp_background, tiling, alignment, area, tab_contents_height);
    }
  }
}
