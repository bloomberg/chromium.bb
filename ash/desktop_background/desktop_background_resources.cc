// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_resources.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "grit/ash_wallpaper_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// Keeps in sync (same order) with WallpaperLayout enum in header file.
const char* kWallpaperLayoutArrays[] = {
    "CENTER",
    "CENTER_CROPPED",
    "STRETCH",
    "TILE"
};

const ash::WallpaperInfo kDefaultWallpapers[] = {
#if defined(GOOGLE_CHROME_BUILD)
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE7_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE7_SMALL,
            ash::CENTER
        }
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE8_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE8_SMALL,
            ash::CENTER
        }
    },
#endif
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT5_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT5_SMALL,
            ash::TILE
        }
    }
};

const int kWallpaperLayoutCount = arraysize(kWallpaperLayoutArrays);
const int kDefaultWallpaperCount = arraysize(kDefaultWallpapers);
const int kInvalidWallpaperIndex = -1;
const int kSolidColorIndex = -2;

// TODO(saintlou): These hardcoded indexes, although checked against the size
// of the array are really hacky.
#if defined(GOOGLE_CHROME_BUILD)
const int kDefaultWallpaperIndex = 1; // IDR_AURA_WALLPAPERS_2_LANDSCAPE8
const int kGuestWallpaperIndex = 0; // IDR_AURA_WALLPAPERS_2_LANDSCAPE7
#else
// Set default wallpaper to the grey background for faster wallpaper loading
// time in browser tests. Otherwise, some of the tests will finish before
// wallpaper loaded and cause crashes.
const int kDefaultWallpaperIndex = 0;  // IDR_AURA_WALLPAPERS_5_GRADIENT5
const int kGuestWallpaperIndex = kDefaultWallpaperIndex;
#endif

}  // namespace

namespace ash {

int GetDefaultWallpaperIndex() {
  DCHECK(kDefaultWallpaperIndex < kDefaultWallpaperCount);
  return std::min(kDefaultWallpaperIndex, kDefaultWallpaperCount - 1);
}

int GetGuestWallpaperIndex() {
  DCHECK(kGuestWallpaperIndex < kDefaultWallpaperCount);
  return std::min(kGuestWallpaperIndex, kDefaultWallpaperCount - 1);
}

int GetInvalidWallpaperIndex() {
  return kInvalidWallpaperIndex;
}

WallpaperLayout GetLayoutEnum(const std::string& layout) {
  for (int i = 0; i < kWallpaperLayoutCount; i++) {
    if (layout.compare(kWallpaperLayoutArrays[i]) == 0)
      return static_cast<WallpaperLayout>(i);
  }
  // Default to use CENTER layout.
  return CENTER;
}

int GetSolidColorIndex() {
  return kSolidColorIndex;
}

int GetWallpaperCount() {
  return kDefaultWallpaperCount;
}

const WallpaperInfo& GetWallpaperInfo(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return kDefaultWallpapers[index];
}

const WallpaperViewInfo& GetWallpaperViewInfo(int index,
                                              WallpaperResolution resolution) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  if (resolution == SMALL)
    return kDefaultWallpapers[index].small_wallpaper;
  else
    return kDefaultWallpapers[index].large_wallpaper;
}

}  // namespace ash
