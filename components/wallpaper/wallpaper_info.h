// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_

#include "base/time/time.h"
#include "components/user_manager/user.h"
#include "components/wallpaper/wallpaper_export.h"

namespace wallpaper {

// This enum is used to back a histogram, and should therefore be treated as
// append-only.
enum WallpaperLayout {
  // Center the wallpaper on the desktop without scaling it. The wallpaper
  // may be cropped.
  WALLPAPER_LAYOUT_CENTER,
  // Scale the wallpaper (while preserving its aspect ratio) to cover the
  // desktop; the wallpaper may be cropped.
  WALLPAPER_LAYOUT_CENTER_CROPPED,
  // Scale the wallpaper (without preserving its aspect ratio) to match the
  // desktop's size.
  WALLPAPER_LAYOUT_STRETCH,
  // Tile the wallpaper over the background without scaling it.
  WALLPAPER_LAYOUT_TILE,
  // This must remain last.
  NUM_WALLPAPER_LAYOUT,
};

struct WALLPAPER_EXPORT WallpaperInfo {
  WallpaperInfo()
      : layout(WALLPAPER_LAYOUT_CENTER),
        type(user_manager::User::WALLPAPER_TYPE_COUNT) {}

  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                user_manager::User::WallpaperType in_type,
                const base::Time& in_date)
      : location(in_location),
        layout(in_layout),
        type(in_type),
        date(in_date) {}

  ~WallpaperInfo() {}

  bool operator==(const WallpaperInfo& other) {
    return (location == other.location) && (layout == other.layout) &&
           (type == other.type);
  }

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id) or online wallpaper URL.
  std::string location;
  WallpaperLayout layout;
  user_manager::User::WallpaperType type;
  base::Time date;
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_
