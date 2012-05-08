// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_

#include "ash/ash_export.h"

class SkBitmap;

namespace ash {

enum ImageLayout {
  CENTER,
  CENTER_CROPPED,
  STRETCH,
  TILE,
};

struct ASH_EXPORT WallpaperInfo {
  int id;
  int thumb_id;
  ImageLayout layout;
  // TODO(bshe): author member should be encoded to UTF16. We need to use i18n
  // string for this member after M19.
  const char* author;
  const char* website;
};

ASH_EXPORT int GetInvalidWallpaperIndex();
ASH_EXPORT int GetDefaultWallpaperIndex();
ASH_EXPORT int GetGuestWallpaperIndex();
ASH_EXPORT int GetRandomWallpaperIndex();
ASH_EXPORT int GetWallpaperCount();
ASH_EXPORT const WallpaperInfo& GetWallpaperInfo(int index);

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
