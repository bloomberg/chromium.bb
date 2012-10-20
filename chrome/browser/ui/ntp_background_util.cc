// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ntp_background_util.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "grit/theme_resources.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/skia_util.h"

namespace {

int Round(float value) {
  return gfx::ToRoundedInt(value + 0.5);
}

void PaintThemeBackground(
    gfx::Canvas* canvas,
    gfx::ImageSkia* ntp_background,
    int tiling,
    int alignment,
    const gfx::Rect& area_in_canvas,
    int tab_contents_height,
    const gfx::Size& browser_client_area_size,
    const gfx::Point& area_origin_in_browser_client_area) {
  int x_pos = 0;
  int y_pos = 0;
  int width = area_in_canvas.width() + ntp_background->width();
  int height = area_in_canvas.height() + ntp_background->height();

  if (alignment & ThemeService::ALIGN_BOTTOM) {
    y_pos += area_in_canvas.height() + tab_contents_height -
        ntp_background->height();
  } else if (alignment & ThemeService::ALIGN_TOP) {
    // no op
  } else {  // ALIGN_CENTER
    // If there is no |browser_client_area_size|, the theme image only fills up
    // |tab_contents_height|, else it fills up |browser_client_area_size|.
    if (browser_client_area_size.IsEmpty()) {
      y_pos += Round(area_in_canvas.height() + tab_contents_height / 2.0 -
          ntp_background->height() / 2.0);
    } else  {
      y_pos += NtpBackgroundUtil::GetPlacementOfCenterAlignedImage(
          browser_client_area_size.height(), ntp_background->height());
      y_pos -= area_origin_in_browser_client_area.y();
    }
  }

  // Use |browser_client_area_size| for width if it's available, else use
  // |area_in_canvas|'s.
  int width_to_use = browser_client_area_size.IsEmpty() ?
      area_in_canvas.width() : browser_client_area_size.width();
  if (alignment & ThemeService::ALIGN_RIGHT) {
    x_pos += width_to_use - ntp_background->width();
  } else if (alignment & ThemeService::ALIGN_LEFT) {
    // no op
  } else {  // ALIGN_CENTER
    x_pos += NtpBackgroundUtil::GetPlacementOfCenterAlignedImage(
        width_to_use, ntp_background->width());
    x_pos -= area_origin_in_browser_client_area.x();
  }

  if (tiling != ThemeService::REPEAT &&
      tiling != ThemeService::REPEAT_X) {
    width = ntp_background->width();
  } else if (x_pos > 0) {
    x_pos = x_pos % ntp_background->width() - ntp_background->width();
  }

  if (tiling != ThemeService::REPEAT &&
      tiling != ThemeService::REPEAT_Y) {
    height = ntp_background->height();
  } else if (y_pos > 0) {
    y_pos = y_pos % ntp_background->height() - ntp_background->height();
  }

  x_pos += area_in_canvas.x();
  y_pos += area_in_canvas.y();

  // If |x_pos| and/or |y_pos| is/are negative, and/or |width| and/or |height|
  // is/are bigger than size of |area_in_canvas|, the image could be painted
  // outside of |area_in_canvas| in |canvas|.  To make sure we only paint into
  // |area_in_canvas|, clip the area.
  canvas->Save();
  canvas->ClipRect(area_in_canvas);
  canvas->TileImageInt(*ntp_background, x_pos, y_pos, width, height);
  canvas->Restore();
}

void PaintBackground(Profile* profile,
                     gfx::Canvas* canvas,
                     const gfx::Rect& area_in_canvas,
                     int tab_contents_height,
                     const gfx::Size& browser_client_area_size,
                     const gfx::Point& area_origin_in_browser_client_area) {
  // Draw the background to match the new tab page.
  canvas->FillRect(area_in_canvas,
                   chrome::search::GetNTPBackgroundColor(profile));

  const ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile);
  if (tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    int tiling = ThemeService::NO_REPEAT;
    tp->GetDisplayProperty(ThemeService::NTP_BACKGROUND_TILING, &tiling);
    int alignment;
    if (tp->GetDisplayProperty(ThemeService::NTP_BACKGROUND_ALIGNMENT,
                               &alignment)) {
      gfx::ImageSkia* ntp_background =
          tp->GetImageSkiaNamed(IDR_THEME_NTP_BACKGROUND);

      PaintThemeBackground(
          canvas, ntp_background, tiling, alignment, area_in_canvas,
          tab_contents_height, browser_client_area_size,
          area_origin_in_browser_client_area);
    }
  }
}

}  // namespace

// static
void NtpBackgroundUtil::PaintBackgroundDetachedMode(
    Profile* profile,
    gfx::Canvas* canvas,
    const gfx::Rect& area,
    int tab_contents_height) {
  PaintBackground(profile, canvas, area, tab_contents_height, gfx::Size(),
                  gfx::Point());
}

// static
void NtpBackgroundUtil::PaintBackgroundForBrowserClientArea(
    Profile* profile,
    gfx::Canvas* canvas,
    const gfx::Rect& area_in_canvas,
    const gfx::Size& browser_client_area_size,
    const gfx::Rect& area_in_browser_client_area) {
  PaintBackground(profile, canvas, area_in_canvas,
                  browser_client_area_size.height() -
                      area_in_browser_client_area.bottom(),
                  browser_client_area_size,
                  area_in_browser_client_area.origin());
}

// static
int NtpBackgroundUtil::GetPlacementOfCenterAlignedImage(int view_dimension,
                                                        int image_dimension) {
  return Round(view_dimension / 2.0 - image_dimension / 2.0);
}
