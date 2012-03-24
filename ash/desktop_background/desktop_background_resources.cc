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
    {
        IDR_AURA_WALLPAPERS_ROMAINGUY_0,
        IDR_AURA_WALLPAPERS_ROMAINGUY_0_THUMB,
        ash::CENTER_CROPPED,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_0,
        IDR_AURA_WALLPAPERS_WALLPAPER_0_THUMB,
        ash::TILE,
        "Test Gradient 0",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_1,
        IDR_AURA_WALLPAPERS_WALLPAPER_1_THUMB,
        ash::TILE,
        "Test Gradient 1",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_2,
        IDR_AURA_WALLPAPERS_WALLPAPER_2_THUMB,
        ash::TILE,
        "Test Gradient 2",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_3,
        IDR_AURA_WALLPAPERS_WALLPAPER_3_THUMB,
        ash::TILE,
        "Test Gradient 3",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_4,
        IDR_AURA_WALLPAPERS_WALLPAPER_4_THUMB,
        ash::TILE,
        "Test Gradient 4",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_5,
        IDR_AURA_WALLPAPERS_WALLPAPER_5_THUMB,
        ash::TILE,
        "Test Gradient 5",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_6,
        IDR_AURA_WALLPAPERS_WALLPAPER_6_THUMB,
        ash::TILE,
        "Test Gradient 6",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_7,
        IDR_AURA_WALLPAPERS_WALLPAPER_7_THUMB,
        ash::TILE,
        "Test Gradient 7",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_8,
        IDR_AURA_WALLPAPERS_WALLPAPER_8_THUMB,
        ash::TILE,
        "Test Gradient 8",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_9,
        IDR_AURA_WALLPAPERS_WALLPAPER_9_THUMB,
        ash::TILE,
        "Test Gradient 9",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_10,
        IDR_AURA_WALLPAPERS_WALLPAPER_10_THUMB,
        ash::TILE,
        "Test Gradient 10",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_11,
        IDR_AURA_WALLPAPERS_WALLPAPER_11_THUMB,
        ash::TILE,
        "Test Gradient 11",
        "http://www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_12,
        IDR_AURA_WALLPAPERS_WALLPAPER_12_THUMB,
        ash::TILE,
        "Test Gradient 12",
        "http://www.chromium.org"
    },
#if defined(GOOGLE_CHROME_BUILD)
    {
        IDR_AURA_WALLPAPERS_CHARLESDAVEY_0,
        IDR_AURA_WALLPAPERS_CHARLESDAVEY_0_THUMB,
        ash::CENTER_CROPPED,
        "Charles Davey",
        "http://500px.com/CharlesDavey"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_0,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_0_THUMB,
        ash::CENTER_CROPPED,
        "Johannes van Donge",
        "http://www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_1,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_1_THUMB,
        ash::CENTER_CROPPED,
        "Johannes van Donge",
        "http://www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_2,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_2_THUMB,
        ash::CENTER_CROPPED,
        "Johannes van Donge",
        "http://www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_0,
        IDR_AURA_WALLPAPERS_MARIOMORENO_0_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_1,
        IDR_AURA_WALLPAPERS_MARIOMORENO_1_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_2,
        IDR_AURA_WALLPAPERS_MARIOMORENO_2_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_3,
        IDR_AURA_WALLPAPERS_MARIOMORENO_3_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_4,
        IDR_AURA_WALLPAPERS_MARIOMORENO_4_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_5,
        IDR_AURA_WALLPAPERS_MARIOMORENO_5_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_6,
        IDR_AURA_WALLPAPERS_MARIOMORENO_6_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_7,
        IDR_AURA_WALLPAPERS_MARIOMORENO_7_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_8,
        IDR_AURA_WALLPAPERS_MARIOMORENO_8_THUMB,
        ash::CENTER_CROPPED,
        "Mario Moreno",
        "http://www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_0,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_0_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_1,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_1_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_2,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_2_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_3,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_3_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_4,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_4_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_5,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_5_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_6,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_6_THUMB,
        ash::CENTER_CROPPED,
        "Mark Bridger",
        "http://www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_0,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_0_THUMB,
        ash::CENTER_CROPPED,
        "Michel Bricteux",
        "http://500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_1,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_1_THUMB,
        ash::CENTER_CROPPED,
        "Michel Bricteux",
        "http://500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_2,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_2_THUMB,
        ash::CENTER_CROPPED,
        "Michel Bricteux",
        "http://500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_3,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_3_THUMB,
        ash::CENTER_CROPPED,
        "Michel Bricteux",
        "http://500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_0,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_0_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_1,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_1_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_2,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_2_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_3,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_3_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_4,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_4_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_5,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_5_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_6,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_6_THUMB,
        ash::CENTER_CROPPED,
        "Mike Reyfman",
        "http://mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_0,
        IDR_AURA_WALLPAPERS_NEILKREMER_0_THUMB,
        ash::CENTER_CROPPED,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_1,
        IDR_AURA_WALLPAPERS_NEILKREMER_1_THUMB,
        ash::CENTER_CROPPED,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_2,
        IDR_AURA_WALLPAPERS_NEILKREMER_2_THUMB,
        ash::CENTER_CROPPED,
        "Neil Kremer",
        "http://lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_0,
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_0_THUMB,
        ash::CENTER_CROPPED,
        "Oleg Zhukov",
        "http://500px.com/eosboy"
    },
    {
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_1,
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_1_THUMB,
        ash::CENTER_CROPPED,
        "Oleg Zhukov",
        "http://500px.com/eosboy"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_0,
        IDR_AURA_WALLPAPERS_PAULOFLOP_0_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_1,
        IDR_AURA_WALLPAPERS_PAULOFLOP_1_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_2,
        IDR_AURA_WALLPAPERS_PAULOFLOP_2_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_3,
        IDR_AURA_WALLPAPERS_PAULOFLOP_3_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_4,
        IDR_AURA_WALLPAPERS_PAULOFLOP_4_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_5,
        IDR_AURA_WALLPAPERS_PAULOFLOP_5_THUMB,
        ash::CENTER_CROPPED,
        "Paulo FLOP",
        "http://500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_0,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_0_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_1,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_1_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_2,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_2_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_3,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_3_THUMB,
        ash::CENTER_CROPPED,
        "Stefano Ronchi",
        "http://www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_0,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_0_THUMB,
        ash::CENTER_CROPPED,
        "Vitali Prokopenko",
        "http://www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_1,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_1_THUMB,
        ash::CENTER_CROPPED,
        "Vitali Prokopenko",
        "http://www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_2,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_2_THUMB,
        ash::CENTER_CROPPED,
        "Vitali Prokopenko",
        "http://www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_WALTERSOESTBERGEN_0,
        IDR_AURA_WALLPAPERS_WALTERSOESTBERGEN_0_THUMB,
        ash::CENTER_CROPPED,
        "Walter Soestbergen",
        "http://www.waltersoestbergen.nl"
    },
    {
        IDR_AURA_WALLPAPERS_BEACH_DAY,
        IDR_AURA_WALLPAPERS_BEACH_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_BEACH_NIGHT,
        IDR_AURA_WALLPAPERS_BEACH_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_FOREST_DAY,
        IDR_AURA_WALLPAPERS_FOREST_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_FOREST_NIGHT,
        IDR_AURA_WALLPAPERS_FOREST_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_MOUNTAIN_DAY,
        IDR_AURA_WALLPAPERS_MOUNTAIN_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_MOUNTAIN_NIGHT,
        IDR_AURA_WALLPAPERS_MOUNTAIN_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_NYC_DAY,
        IDR_AURA_WALLPAPERS_NYC_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_NYC_NIGHT,
        IDR_AURA_WALLPAPERS_NYC_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_PLAINS_DAY,
        IDR_AURA_WALLPAPERS_PLAINS_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_PLAINS_NIGHT,
        IDR_AURA_WALLPAPERS_PLAINS_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_SF_DAY,
        IDR_AURA_WALLPAPERS_SF_DAY_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
    {
        IDR_AURA_WALLPAPERS_SF_NIGHT,
        IDR_AURA_WALLPAPERS_SF_NIGHT_THUMB,
        // TODO(saintlou): change to ash::CENTER once final assets are added.
        ash::STRETCH,
        "Test",
        "http://www.chromium.org",
    },
#endif  // GOOGLE_CHROME_BUILD
};

const int kDefaultWallpaperCount = arraysize(kDefaultWallpapers);

// TODO(saintlou): These hardcoded indexes, although checked against the size
// of the array are really hacky.
#if defined(GOOGLE_CHROME_BUILD)
const int kLastRandomWallpaperIndex = 12;
const int kIcognitoWallpaperIndex = 0;
#else
const int kDefaultWallpaperIndex = 0;
const int kIcognitoWallpaperIndex = kDefaultWallpaperIndex;
#endif

}  // namespace

namespace ash {

int GetDefaultWallpaperIndex() {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(kLastRandomWallpaperIndex < kDefaultWallpaperCount);
  return base::RandInt(0,
      std::min(kLastRandomWallpaperIndex, kDefaultWallpaperCount - 1));
#else
  return kDefaultWallpaperIndex;
#endif
}

int GetGuestWallpaperIndex() {
  DCHECK(kIcognitoWallpaperIndex < kDefaultWallpaperCount);
  return std::min(kIcognitoWallpaperIndex, kDefaultWallpaperCount - 1);
}

int GetWallpaperCount() {
  return kDefaultWallpaperCount;
}

const SkBitmap& GetWallpaper(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kDefaultWallpapers[index].id).ToSkBitmap();
}

const SkBitmap& GetWallpaperThumbnail(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kDefaultWallpapers[index].thumb_id).ToSkBitmap();
}

const WallpaperInfo& GetWallpaperInfo(int index) {
  return kDefaultWallpapers[index];
}

}  // namespace ash
