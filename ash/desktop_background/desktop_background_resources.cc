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

struct WallpaperInfo {
  int id;
  int thumb_id;
  const char* author;
  const char* url;
};

const WallpaperInfo kDefaultWallpapers[] = {
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_0,
        IDR_AURA_WALLPAPERS_WALLPAPER_0_THUMB,
        "Test Gradient 0",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_1,
        IDR_AURA_WALLPAPERS_WALLPAPER_1_THUMB,
        "Test Gradient 1",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_2,
        IDR_AURA_WALLPAPERS_WALLPAPER_2_THUMB,
        "Test Gradient 2",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_3,
        IDR_AURA_WALLPAPERS_WALLPAPER_3_THUMB,
        "Test Gradient 3",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_4,
        IDR_AURA_WALLPAPERS_WALLPAPER_4_THUMB,
        "Test Gradient 4",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_5,
        IDR_AURA_WALLPAPERS_WALLPAPER_5_THUMB,
        "Test Gradient 5",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_6,
        IDR_AURA_WALLPAPERS_WALLPAPER_6_THUMB,
        "Test Gradient 6",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_7,
        IDR_AURA_WALLPAPERS_WALLPAPER_7_THUMB,
        "Test Gradient 7",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_8,
        IDR_AURA_WALLPAPERS_WALLPAPER_8_THUMB,
        "Test Gradient 8",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_9,
        IDR_AURA_WALLPAPERS_WALLPAPER_9_THUMB,
        "Test Gradient 9",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_10,
        IDR_AURA_WALLPAPERS_WALLPAPER_10_THUMB,
        "Test Gradient 10",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_11,
        IDR_AURA_WALLPAPERS_WALLPAPER_11_THUMB,
        "Test Gradient 11",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_WALLPAPER_12,
        IDR_AURA_WALLPAPERS_WALLPAPER_12_THUMB,
        "Test Gradient 0",
        "http//www.chromium.org"
    },
    {
        IDR_AURA_WALLPAPERS_ROMAINGUY_0,
        IDR_AURA_WALLPAPERS_ROMAINGUY_0_THUMB,
        "Romain Guy",
        "http://www.curious-creature.org"
    },
#if defined(GOOGLE_CHROME_BUILD)
    {
        IDR_AURA_WALLPAPERS_CHARLESDAVEY_0,
        IDR_AURA_WALLPAPERS_CHARLESDAVEY_0_THUMB,
        "Charles Davey",
        "http//500px.com/CharlesDavey"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_0,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_0_THUMB,
        "Johannes van Donge",
        "http//www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_1,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_1_THUMB,
        "Johannes van Donge",
        "http//www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_2,
        IDR_AURA_WALLPAPERS_JOHANNESVANDONGE_2_THUMB,
        "Johannes van Donge",
        "http//www.diginature.nl"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_0,
        IDR_AURA_WALLPAPERS_MARIOMORENO_0_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_1,
        IDR_AURA_WALLPAPERS_MARIOMORENO_1_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_2,
        IDR_AURA_WALLPAPERS_MARIOMORENO_2_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_3,
        IDR_AURA_WALLPAPERS_MARIOMORENO_3_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_4,
        IDR_AURA_WALLPAPERS_MARIOMORENO_4_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_5,
        IDR_AURA_WALLPAPERS_MARIOMORENO_5_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_6,
        IDR_AURA_WALLPAPERS_MARIOMORENO_6_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_7,
        IDR_AURA_WALLPAPERS_MARIOMORENO_7_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARIOMORENO_8,
        IDR_AURA_WALLPAPERS_MARIOMORENO_8_THUMB,
        "Mario Moreno",
        "http//www.mariomorenophotography.com"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_0,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_0_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_1,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_1_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_2,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_2_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_3,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_3_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_4,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_4_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_5,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_5_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MARKBRIDGER_6,
        IDR_AURA_WALLPAPERS_MARKBRIDGER_6_THUMB,
        "Mark Bridger",
        "http//www.bridgephotography.co.uk"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_0,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_0_THUMB,
        "Michel Bricteux",
        "http//500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_1,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_1_THUMB,
        "Michel Bricteux",
        "http//500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_2,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_2_THUMB,
        "Michel Bricteux",
        "http//500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_3,
        IDR_AURA_WALLPAPERS_MICHELBRICTEUX_3_THUMB,
        "Michel Bricteux",
        "http//500px.com/mbricteux"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_0,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_0_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_1,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_1_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_2,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_2_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_3,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_3_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_4,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_4_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_5,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_5_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_6,
        IDR_AURA_WALLPAPERS_MIKEREYFMAN_6_THUMB,
        "Mike Reyfman",
        "http//mikereyfman.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_0,
        IDR_AURA_WALLPAPERS_NEILKREMER_0_THUMB,
        "Neil Kremer",
        "http//lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_1,
        IDR_AURA_WALLPAPERS_NEILKREMER_1_THUMB,
        "Neil Kremer",
        "http//lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_NEILKREMER_2,
        IDR_AURA_WALLPAPERS_NEILKREMER_2_THUMB,
        "Neil Kremer",
        "http//lightshedimagery.smugmug.com"
    },
    {
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_0,
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_0_THUMB,
        "Oleg Zhukov",
        "http//500px.com/eosboy"
    },
    {
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_1,
        IDR_AURA_WALLPAPERS_OLEGZHUKOV_1_THUMB,
        "Oleg Zhukov",
        "http//500px.com/eosboy"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_0,
        IDR_AURA_WALLPAPERS_PAULOFLOP_0_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_1,
        IDR_AURA_WALLPAPERS_PAULOFLOP_1_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_2,
        IDR_AURA_WALLPAPERS_PAULOFLOP_2_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_3,
        IDR_AURA_WALLPAPERS_PAULOFLOP_3_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_4,
        IDR_AURA_WALLPAPERS_PAULOFLOP_4_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_PAULOFLOP_5,
        IDR_AURA_WALLPAPERS_PAULOFLOP_5_THUMB,
        "Paulo FLOP",
        "http//500px.com/FLOP"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_0,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_0_THUMB,
        "Stefano Ronchi",
        "http//www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_1,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_1_THUMB,
        "Stefano Ronchi",
        "http//www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_2,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_2_THUMB,
        "Stefano Ronchi",
        "http//www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_STEFANORONCHI_3,
        IDR_AURA_WALLPAPERS_STEFANORONCHI_3_THUMB,
        "Stefano Ronchi",
        "http//www.stefanoronchi.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_0,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_0_THUMB,
        "Vitali Prokopenko",
        "http//www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_1,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_1_THUMB,
        "Vitali Prokopenko",
        "http//www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_2,
        IDR_AURA_WALLPAPERS_VITALIPROKOPENKO_2_THUMB,
        "Vitali Prokopenko",
        "http//www.vitphoto.com"
    },
    {
        IDR_AURA_WALLPAPERS_WALTERSOESTBERGEN_0,
        IDR_AURA_WALLPAPERS_WALTERSOESTBERGEN_0_THUMB,
        "Walter Soestbergen",
        "http//www.waltersoestbergen.nl"
    },
#endif  // GOOGLE_CHROME_BUILD
};

const int kDefaultWallpaperCount = arraysize(kDefaultWallpapers);
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
      kDefaultWallpapers[index].id).ToSkBitmap();
}

const SkBitmap& GetWallpaperThumbnail(int index) {
  DCHECK(index >= 0 && index < kDefaultWallpaperCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kDefaultWallpapers[index].thumb_id).ToSkBitmap();
}

}  // namespace ash
