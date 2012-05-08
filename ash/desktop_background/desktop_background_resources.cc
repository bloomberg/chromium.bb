// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_resources.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const ash::WallpaperInfo kDefaultWallpapers[] = {
#if !defined(GOOGLE_CHROME_BUILD)
    {
        IDR_AURA_WALLPAPERS_ROMAINGUY_0,
        IDR_AURA_WALLPAPERS_ROMAINGUY_0_THUMB,
        ash::CENTER_CROPPED,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
#else
    {
        IDR_AURA_WALLPAPERS_1_NATURE0,
        IDR_AURA_WALLPAPERS_1_NATURE0_THUMB,
        ash::CENTER_CROPPED,
        "Kathy Collins / Getty Images",
        "http://www.gettyimages.com"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE1,
        IDR_AURA_WALLPAPERS_1_NATURE1_THUMB,
        ash::CENTER_CROPPED,
        "Johannes van Donge",
        "http://www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE2,
        IDR_AURA_WALLPAPERS_1_NATURE2_THUMB,
        ash::CENTER_CROPPED,
        "Oleg Zhukov",
        "http://500px.com/eosboy"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE3,
        IDR_AURA_WALLPAPERS_1_NATURE3_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE4,
        IDR_AURA_WALLPAPERS_1_NATURE4_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE5,
        IDR_AURA_WALLPAPERS_1_NATURE5_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE6,
        IDR_AURA_WALLPAPERS_1_NATURE6_THUMB,
        ash::CENTER_CROPPED,
        "Walter Soestbergen",
        "http://www.waltersoestbergen.nl"
    },
    {
        IDR_AURA_WALLPAPERS_1_NATURE7,
        IDR_AURA_WALLPAPERS_1_NATURE7_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE0,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE0_THUMB,
        ash::CENTER_CROPPED,
        "Vitali Prokopenko",
        "http://www.vitphoto.com/"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE1,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE1_THUMB,
        ash::CENTER_CROPPED,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE2,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE2_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE3,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE3_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE4,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE4_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE5,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE5_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE6,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE6_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_2_LANDSCAPE7,
        IDR_AURA_WALLPAPERS_2_LANDSCAPE7_THUMB,
        ash::CENTER_CROPPED,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
    {
        IDR_AURA_WALLPAPERS_3_URBAN0,
        IDR_AURA_WALLPAPERS_3_URBAN0_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_3_URBAN1,
        IDR_AURA_WALLPAPERS_3_URBAN1_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_3_URBAN2,
        IDR_AURA_WALLPAPERS_3_URBAN2_THUMB,
        ash::CENTER_CROPPED,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_3_URBAN3,
        IDR_AURA_WALLPAPERS_3_URBAN3_THUMB,
        ash::CENTER_CROPPED,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
#endif
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT0,
        IDR_AURA_WALLPAPERS_5_GRADIENT0_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT1,
        IDR_AURA_WALLPAPERS_5_GRADIENT1_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT2,
        IDR_AURA_WALLPAPERS_5_GRADIENT2_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT3,
        IDR_AURA_WALLPAPERS_5_GRADIENT3_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT4,
        IDR_AURA_WALLPAPERS_5_GRADIENT4_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT5,
        IDR_AURA_WALLPAPERS_5_GRADIENT5_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT6,
        IDR_AURA_WALLPAPERS_5_GRADIENT6_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_5_GRADIENT7,
        IDR_AURA_WALLPAPERS_5_GRADIENT7_THUMB,
        ash::TILE,
        "Chromium",
        "http://www.chromium.org"
    },
};

const int kDefaultWallpaperCount = arraysize(kDefaultWallpapers);
const int kInvalidWallpaperIndex = -1;

// TODO(saintlou): These hardcoded indexes, although checked against the size
// of the array are really hacky.
#if defined(GOOGLE_CHROME_BUILD)
const int kDefaultWallpaperIndex = 16; // IDR_AURA_WALLPAPERS_3_URBAN0
const int kLastRandomWallpaperIndex = 19; // The first 20 are random.
const int kGuestWallpaperIndex = 26;   // IDR_AURA_WALLPAPERS_5_GRADIENT6
#else
// Set default wallpaper to the grey background for faster wallpaper loading
// time in browser tests. Otherwise, some of the tests will finish before
// wallpaper loaded and cause crashes.
const int kDefaultWallpaperIndex = 6;  // IDR_AURA_WALLPAPERS_5_GRADIENT5
const int kLastRandomWallpaperIndex = 8;
const int kGuestWallpaperIndex = kDefaultWallpaperIndex;
#endif

}  // namespace

namespace ash {

int GetInvalidWallpaperIndex() {
  return kInvalidWallpaperIndex;
}

int GetDefaultWallpaperIndex() {
  DCHECK(kDefaultWallpaperIndex < kDefaultWallpaperCount);
  return std::min(kDefaultWallpaperIndex, kDefaultWallpaperCount - 1);
}

int GetGuestWallpaperIndex() {
  DCHECK(kGuestWallpaperIndex < kDefaultWallpaperCount);
  return std::min(kGuestWallpaperIndex, kDefaultWallpaperCount - 1);
}

int GetRandomWallpaperIndex() {
  DCHECK(kLastRandomWallpaperIndex < kDefaultWallpaperCount);
  return base::RandInt(0,
      std::min(kLastRandomWallpaperIndex, kDefaultWallpaperCount - 1));
}

int GetWallpaperCount() {
  return kDefaultWallpaperCount;
}

const WallpaperInfo& GetWallpaperInfo(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return kDefaultWallpapers[index];
}

}  // namespace ash
