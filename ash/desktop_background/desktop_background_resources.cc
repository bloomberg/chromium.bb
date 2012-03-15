// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_resources.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const int kDefaultWallpaperResources[] = {
  IDR_AURA_WALLPAPER,
  IDR_AURA_WALLPAPER_1,
  IDR_AURA_WALLPAPER_2,
  IDR_AURA_WALLPAPER_3,
  IDR_AURA_WALLPAPER_4,
  IDR_AURA_WALLPAPER_5,
  IDR_AURA_WALLPAPER_6,
  IDR_AURA_WALLPAPER_7,
  IDR_AURA_WALLPAPER_8,
};

const int kDefaultWallpaperResourcesThumb[] = {
  IDR_AURA_WALLPAPER_THUMB,
  IDR_AURA_WALLPAPER_1_THUMB,
  IDR_AURA_WALLPAPER_2_THUMB,
  IDR_AURA_WALLPAPER_3_THUMB,
  IDR_AURA_WALLPAPER_4_THUMB,
  IDR_AURA_WALLPAPER_5_THUMB,
  IDR_AURA_WALLPAPER_6_THUMB,
  IDR_AURA_WALLPAPER_7_THUMB,
  IDR_AURA_WALLPAPER_8_THUMB,
};

const int kDefaultWallpaperCount = arraysize(kDefaultWallpaperResources);

const int kDefaultWallpaperIndex = 0;

}  // namespace

namespace ash {

int GetDefaultWallpaperIndex() {
  return kDefaultWallpaperIndex;
}

int GetWallpaperCount() {
  return kDefaultWallpaperCount;
}

const SkBitmap& GetWallpaper(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kDefaultWallpaperResources[index]).ToSkBitmap();
}

const SkBitmap& GetWallpaperThumbnail(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kDefaultWallpaperResourcesThumb[index]).ToSkBitmap();
}

}  // namespace ash
