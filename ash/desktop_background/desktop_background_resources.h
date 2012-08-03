// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_

#include <string>

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

enum WallpaperLayout {
  CENTER,
  CENTER_CROPPED,
  STRETCH,
  TILE,
};

enum WallpaperResolution {
  LARGE,
  SMALL
};

// Encapsulates wallpaper infomation needed by desktop background view.
struct ASH_EXPORT WallpaperViewInfo {
  int id;
  WallpaperLayout layout;
};

struct ASH_EXPORT WallpaperInfo {
  WallpaperViewInfo large_wallpaper;
  WallpaperViewInfo small_wallpaper;
  int thumb_id;
  // TODO(bshe): author member should be encoded to UTF16. We need to use i18n
  // string for this member after M19.
  const char* author;
  const char* website;
};

const SkColor kLoginWallpaperColor = 0xFEFEFE;

ASH_EXPORT int GetDefaultWallpaperIndex();
ASH_EXPORT int GetGuestWallpaperIndex();
ASH_EXPORT int GetInvalidWallpaperIndex();
ASH_EXPORT WallpaperLayout GetLayoutEnum(const std::string& layout);
ASH_EXPORT int GetNextWallpaperIndex(int index);
ASH_EXPORT int GetSolidColorIndex();
ASH_EXPORT int GetWallpaperCount();
ASH_EXPORT const WallpaperInfo& GetWallpaperInfo(int index);
ASH_EXPORT const WallpaperViewInfo& GetWallpaperViewInfo(int index,
    WallpaperResolution resolution);

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
