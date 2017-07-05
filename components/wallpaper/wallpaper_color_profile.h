// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_COLOR_PROFILE_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_COLOR_PROFILE_H_

namespace wallpaper {

// The color profile type, ordered as the color profiles applied in
// ash::WallpaperController.
enum class ColorProfileType {
  DARK_VIBRANT = 0,
  NORMAL_VIBRANT,
  LIGHT_VIBRANT,
  DARK_MUTED,
  NORMAL_MUTED,
  LIGHT_MUTED,

  NUM_OF_COLOR_PROFILES,
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_COLOR_PROFILE_H_
