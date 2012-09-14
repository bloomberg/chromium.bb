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
#if !defined(GOOGLE_CHROME_BUILD)
    {
        {
            IDR_AURA_WALLPAPERS_ROMAINGUY_0_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_ROMAINGUY_0_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_ROMAINGUY_0_THUMB,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
#else
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE0_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE0_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE0_THUMB,
        "Kathy Collins / Getty Images",
        "http://www.gettyimages.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE1_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE1_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE1_THUMB,
        "Johannes van Donge",
        "http://www.diginature.nl"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE2_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE2_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE2_THUMB,
        "Oleg Zhukov",
        "http://500px.com/eosboy"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE3_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE3_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE3_THUMB,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE4_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE4_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE4_THUMB,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE5_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE5_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE5_THUMB,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE6_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE6_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE6_THUMB,
        "Walter Soestbergen",
        "http://www.waltersoestbergen.nl"
    },
    {
        {
            IDR_AURA_WALLPAPERS_1_NATURE7_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_1_NATURE7_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_1_NATURE7_THUMB,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE0_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE0_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE0_THUMB,
        "Vitali Prokopenko",
        "http://www.vitphoto.com/"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE1_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE1_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE1_THUMB,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE2_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE2_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE2_THUMB,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE3_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE3_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE3_THUMB,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE4_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE4_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE4_THUMB,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE5_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE5_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE5_THUMB,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE6_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE6_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE6_THUMB,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE7_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE7_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE7_THUMB,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_3_URBAN0_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_3_URBAN0_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_3_URBAN0_THUMB,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        {
            IDR_AURA_WALLPAPERS_3_URBAN1_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_3_URBAN1_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_3_URBAN1_THUMB,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_3_URBAN2_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_3_URBAN2_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_3_URBAN2_THUMB,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_3_URBAN3_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_3_URBAN3_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_3_URBAN3_THUMB,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE8_LARGE,
            ash::CENTER_CROPPED
        },
        {
            IDR_AURA_WALLPAPERS_2_LANDSCAPE8_SMALL,
            ash::CENTER
        },
        IDR_AURA_WALLPAPERS_2_LANDSCAPE8_THUMB,
        "Clemens Günthermann",
        "http://www.clegue.com"
    },
#endif
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT1_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT1_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT1_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT2_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT2_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT2_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT3_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT3_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT3_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT4_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT4_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT4_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT5_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT5_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT5_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT6_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT6_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT6_THUMB,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT7_LARGE,
            ash::TILE
        },
        {
            IDR_AURA_WALLPAPERS_5_GRADIENT7_SMALL,
            ash::TILE
        },
        IDR_AURA_WALLPAPERS_5_GRADIENT7_THUMB,
        "Chromium",
        "http://www.chromium.org"
    }
};

const int kWallpaperLayoutCount = arraysize(kWallpaperLayoutArrays);
const int kDefaultWallpaperCount = arraysize(kDefaultWallpapers);
const int kInvalidWallpaperIndex = -1;
const int kSolidColorIndex = -2;

// TODO(saintlou): These hardcoded indexes, although checked against the size
// of the array are really hacky.
#if defined(GOOGLE_CHROME_BUILD)
const int kDefaultWallpaperIndex = 20; // IDR_AURA_WALLPAPERS_2_LANDSCAPE8
const int kLastRandomWallpaperIndex = 19; // The first 20 are random.
const int kGuestWallpaperIndex = kDefaultWallpaperIndex;
#else
// Set default wallpaper to the grey background for faster wallpaper loading
// time in browser tests. Otherwise, some of the tests will finish before
// wallpaper loaded and cause crashes.
const int kDefaultWallpaperIndex = 5;  // IDR_AURA_WALLPAPERS_5_GRADIENT5
const int kLastRandomWallpaperIndex = 8;
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

int GetNextWallpaperIndex(int index) {
  DCHECK(kLastRandomWallpaperIndex < kDefaultWallpaperCount);
  return (index + 1) % (kLastRandomWallpaperIndex + 1);
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
  if (resolution == SMALL)
    return kDefaultWallpapers[index].small_wallpaper;
  else
    return kDefaultWallpapers[index].large_wallpaper;
}

}  // namespace ash
