// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace debug {
namespace {

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control
  // wallpaper images for testing.
  // The size is randomly picked.
  gfx::Size image_size(1366, 768);
  gfx::Canvas canvas(image_size, 1.0f, true);
  canvas.DrawColor(fill);
  SkPaint paint;
  paint.setColor(rect);
  paint.setStrokeWidth(10);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas.DrawRoundRect(gfx::Rect(image_size), 100, paint);
  return gfx::ImageSkia(canvas.ExtractImageRep());
}

}  // namespace

bool CycleDesktopBackgroundMode() {
  static int index = 0;
  DesktopBackgroundController* desktop_background_controller =
      Shell::GetInstance()->desktop_background_controller();
  switch (++index % 4) {
    case 0:
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          InitializeWallpaper();
      break;
    case 1:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE),
          WALLPAPER_LAYOUT_STRETCH);
      break;
    case 2:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN),
          WALLPAPER_LAYOUT_CENTER);
      break;
    case 3:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED),
          WALLPAPER_LAYOUT_CENTER_CROPPED);
      break;
  }
  return true;
}

}  // namespace debug
}  // namespace ash
