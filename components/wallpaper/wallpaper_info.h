// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_

#include "base/time/time.h"
#include "components/wallpaper/wallpaper_export.h"

namespace wallpaper {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   (b) new constants should only be appended at the end of the enumeration.
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

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   (b) new constants should only be appended at the end of the enumeration.
enum WallpaperType {
  DAILY = 0,         // Surprise wallpaper. Changes once a day if enabled.
  CUSTOMIZED = 1,    // Selected by user.
  DEFAULT = 2,       // Default.
  /* UNKNOWN = 3 */  // Removed.
  ONLINE = 4,        // WallpaperInfo.location denotes an URL.
  POLICY = 5,        // Controlled by policy, can't be changed by the user.
  THIRDPARTY = 6,    // Current wallpaper is set by a third party app.
  DEVICE = 7,        // Current wallpaper is the device policy controlled
                     // wallpaper. It shows on the login screen if the device
                     // is an enterprise managed device.
  WALLPAPER_TYPE_COUNT = 8
};

struct WALLPAPER_EXPORT WallpaperInfo {
  WallpaperInfo()
      : layout(WALLPAPER_LAYOUT_CENTER), type(WALLPAPER_TYPE_COUNT) {}

  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                WallpaperType in_type,
                const base::Time& in_date)
      : location(in_location),
        layout(in_layout),
        type(in_type),
        date(in_date) {}

  ~WallpaperInfo() {}

  bool operator==(const WallpaperInfo& other) const {
    return (location == other.location) && (layout == other.layout) &&
           (type == other.type);
  }

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id) or online wallpaper URL.
  std::string location;
  WallpaperLayout layout;
  WallpaperType type;
  base::Time date;
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_INFO_H_
