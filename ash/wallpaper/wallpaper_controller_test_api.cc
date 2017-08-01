// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

WallpaperControllerTestApi::WallpaperControllerTestApi(
    WallpaperController* controller)
    : controller_(controller) {}

WallpaperControllerTestApi::~WallpaperControllerTestApi() {}

SkColor WallpaperControllerTestApi::ApplyColorProducingWallpaper() {
  const SkColor color = SkColorSetRGB(60, 40, 40);
  const SkColor expected_color = SkColorSetRGB(30, 20, 20);

  gfx::Canvas canvas(gfx::Size(5, 5), 1.0f, true);
  canvas.DrawColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(canvas.GetBitmap());
  controller_->SetWallpaperImage(image, wallpaper::WALLPAPER_LAYOUT_CENTER);

  return expected_color;
}

}  // namespace ash
